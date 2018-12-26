#include <arcan_shmif.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/wait.h>

#include "a12_int.h"
#include "a12.h"
#include "a12_helper.h"

static const short c_inev = POLLIN | POLLERR | POLLNVAL | POLLHUP;
static const short c_outev = POLLOUT | POLLERR | POLLNVAL | POLLHUP;

/*
 * Figure out encoding parameters based on client type and buffer parameters.
 * This is the first heurstic catch-all to later feed in backpressure,
 * bandwidth and load estimation parameters.
 */
static struct a12_vframe_opts vopts_from_segment(
	struct shmifsrv_client* C, struct shmifsrv_vbuffer vb)
{
	switch (shmifsrv_client_type(C)){
	case SEGID_LWA:
		return (struct a12_vframe_opts){
			.method = VFRAME_METHOD_H264,
			.bias = VFRAME_BIAS_BALANCED
		};
	case SEGID_GAME:
		return (struct a12_vframe_opts){
			.method = VFRAME_METHOD_H264,
			.bias = VFRAME_BIAS_LATENCY
		};
	break;
	case SEGID_MEDIA:
		return (struct a12_vframe_opts){
			.method = VFRAME_METHOD_H264,
			.bias = VFRAME_BIAS_QUALITY
		};
	break;
	case SEGID_REMOTING:
	case SEGID_VM:
	default:
		return (struct a12_vframe_opts){
			.method = VFRAME_METHOD_NORMAL
		};
	break;
	}
}

int a12helper_poll_triple(int fd_shmif, int fd_in, int fd_out, int timeout)
{
/* 1. setup a12 in connect mode, _open */
	struct pollfd fds[3] = {
		{ .fd = fd_shmif, .events = c_inev },
		{	.fd = fd_in, .events = c_inev },
		{ .fd = fd_out, .events = c_outev }
	};

	int sv = poll(fds, fd_out != -1 ? 3 : 2, timeout);
	if (sv < 0){
		if (sv == -1 && errno != EAGAIN && errno != EINTR)
			return -1;
	}

	if (fds[0].revents & (POLLERR | POLLNVAL | POLLHUP)){
		debug_print(1, "shmif descriptor died");
		return -1;
	}

	if (fds[1].revents & (POLLERR | POLLNVAL | POLLHUP)){
		debug_print(1, "incoming descriptor died");
		return -1;
	}

	if (fd_out != -1 && (fds[2].revents & (POLLERR | POLLNVAL | POLLHUP))){
		debug_print(1, "outgoing descriptor died");
		return -1;
	}

	int res = 0;
	if (fds[0].revents & POLLIN)
		res |= A12HELPER_POLL_SHMIF;

	if (fds[1].revents & POLLIN)
		res |= A12HELPER_DATA_IN;

	if (fd_out != -1 && (fds[2].revents & POLLOUT))
		res |= A12HELPER_WRITE_OUT;

	return res;
}

static void on_srv_event(
	struct arcan_shmif_cont* cont, int chid, struct arcan_event* ev, void* tag)
{
	struct shmifsrv_client* cs = tag;
	debug_print(2,
		"client event: %s on ch %d", arcan_shmif_eventstr(ev, NULL, 0), chid);

	if (chid != 0){
		debug_print(1, "couldn't decode incoming event, invalid channel: %d", chid);
		return;
	}

/* note, this needs to be able to buffer etc. to handle a client that has
 * a saturated event queue ... */
	shmifsrv_enqueue_event(cs, ev, -1);
}

void a12helper_a12cl_shmifsrv(struct a12_state* S,
	struct shmifsrv_client* C, int fd_in, int fd_out, struct a12helper_opts opts)
{

	uint8_t* outbuf;
	size_t outbuf_sz = 0;
	int status;

	while (-1 != (status = a12helper_poll_triple(
		shmifsrv_client_handle(C), fd_in, outbuf_sz ? fd_out : -1, 4))){

/* first, flush current outgoing and/or swap buffers */
		if (status & A12HELPER_WRITE_OUT){
			if (outbuf_sz || (outbuf_sz = a12_channel_flush(S, &outbuf))){
				ssize_t nw = write(fd_out, outbuf, outbuf_sz);

				if (nw > 0){
					outbuf += nw;
					outbuf_sz -= nw;
				}
			}
		}

		if (status & A12HELPER_DATA_IN){
			uint8_t inbuf[9000];
			ssize_t nr = read(fd_in, inbuf, 9000);
			if (-1 == nr && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR){
				break;
			}

			debug_print(2, "unpack %zd bytes", nr);
			a12_channel_unpack(S, inbuf, nr, C, on_srv_event);
		}

/* always poll shmif- when we are here */
		struct arcan_event ev;
		while (shmifsrv_dequeue_events(C, &ev, 1)){
			if (arcan_shmif_descrevent(&ev)){
				debug_print(1, "ignoring descriptor passing event");
			}
			else if (!shmifsrv_process_event(C, &ev)){
				debug_print(2, "forward: %s", arcan_shmif_eventstr(&ev, NULL, 0));
				a12_channel_enqueue(S, &ev);
			}
			else
				debug_print(1, "consumed: %s", arcan_shmif_eventstr(&ev, NULL, 0));
		}

		int pv;
		while ((pv = shmifsrv_poll(C)) != CLIENT_NOT_READY){
			debug_print(1, "client polled to %d", pv);
			if (pv == CLIENT_DEAD){
/* FIXME: shmif-client died, send disconnect packages so we do this cleanly */
				debug_print(1, "client died");
				goto out;
			}
			if (pv & CLIENT_VBUFFER_READY){
/* two option, one is to map the dma-buf ourselves and do the readback,
 * or with streams map the stream and convert to h264 on gpu, but easiest
 * now is to just reject and let the caller do the readback */
/*
 * arcan_event ev = {
	     .category = EVENT_TARGET,
			 .tgt.kind = TARGET_COMMAND_BUFFER_FAIL
	 };
 */
				debug_print(2, "video-buffer");
				struct shmifsrv_vbuffer vb = shmifsrv_video(C);
				a12_channel_vframe(S, 0, &vb, vopts_from_segment(C, vb));
				shmifsrv_video_step(C);
			}
			if (pv & CLIENT_ABUFFER_READY){
				debug_print(2, "audio-buffer");
				shmifsrv_audio(C, NULL, NULL);
			}
		}

		if (!outbuf_sz){
			outbuf_sz = a12_channel_flush(S, &outbuf);
			if (outbuf_sz)
				debug_print(1, "pass over, got: %zu left", outbuf_sz);
		}
	}

out:
#ifdef DUMP_IN
	fclose(fpek_in);
#endif
	debug_print(1, "(srv) shutting down connection");
	shmifsrv_free(C);
}
