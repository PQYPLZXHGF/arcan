/*
 * Copyright 2016, Björn Ståhl
 * License: 3-Clause BSD, see COPYING file in arcan source repository.
 * Reference: http://arcan-fe.com
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <setjmp.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "arcan_math.h"
#include "arcan_general.h"
#include "arcan_db.h"
#include "arcan_shmif.h"
#include "arcan_shmif_sub.h"
#include "arcan_event.h"
#include "arcan_video.h"
#include "arcan_videoint.h"
#include "arcan_audio.h"
#include "arcan_audioint.h"
#include "arcan_3dbase.h"

#define FRAMESERVER_PRIVATE
#include "arcan_frameserver.h"
#include "arcan_vr.h"

#ifdef _DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define debug_print(fmt, ...) \
            do { if (DEBUG) arcan_warning("%s:%d:%s(): " fmt "\n", \
						"arcan_vr:", __LINE__, __func__,##__VA_ARGS__); } while (0)

/*
 * left: metadata, samples, ...
 */
struct limb_ent {
	arcan_vobj_id map;
	uint_least32_t ts;
};

struct arcan_vr_ctx {
	arcan_evctx* ctx;
	arcan_frameserver* connection;
	uint64_t map;
	struct limb_ent limb_map[LIMB_LIM];
};

struct arcan_vr_ctx* arcan_vr_setup(
	const char* bridge_arg, struct arcan_evctx* evctx, uintptr_t tag)
{
	const char* appl;
	struct arcan_dbh* dbh = arcan_db_get_shared(&appl);

	char* kv = arcan_db_appl_val(dbh, appl, "ext_vr");
	if (!kv)
		return NULL;

/*
 * build a frameserver context and envp with the data for the vrbridge
 * and with the subprotocol permissions enabled
 */
	struct arcan_strarr arr_argv = {0}, arr_env = {0};
	arcan_mem_growarr(&arr_argv);
	arr_argv.data[0] = strdup(kv);
	arr_argv.count = 1;

	char* kvd = arcan_db_appl_val(dbh, appl, "ext_vr_debug");
	if (kvd){
		arcan_mem_growarr(&arr_env);
		arr_env.data[0] = strdup("ARCAN_VR_DEBUGATTACH=1");
		arr_env.count = 1;
		free(kvd);
	}

/*
 * There might be some merit to enabling the VOBJ- substructure as well since
 * some providers might want to come with distortion meshes or reference-
 * models for tool-limbs
 */
	struct frameserver_envp args = {
		.metamask = SHMIF_META_VR | SHMIF_META_VOBJ,
		.use_builtin = false,
		.args.external.fname = kv,
		.args.external.envv = &arr_env,
		.args.external.argv = &arr_argv,
		.args.external.resource = strdup(bridge_arg)
	};
	struct arcan_vr_ctx* vrctx = arcan_alloc_mem(
		sizeof(struct arcan_vr_ctx), ARCAN_MEM_VSTRUCT,
		ARCAN_MEM_BZERO | ARCAN_MEM_NONFATAL, ARCAN_MEMALIGN_NATURAL
	);

	if (!vrctx)
		return NULL;

	struct arcan_frameserver* mvctx = platform_launch_fork(&args, tag);
	arcan_mem_freearr(&arr_argv);
	arcan_mem_freearr(&arr_env);
	free(args.args.external.resource);

	if (!mvctx){
		debug_print("couldn't spawn vrbridge");
		arcan_mem_free(vrctx);
		arcan_mem_free(mvctx);
		return NULL;
	}

	debug_print("vrbridge launched");
	*vrctx = (struct arcan_vr_ctx){
		.ctx = evctx,
		.connection = mvctx
	};
	mvctx->segid = SEGID_SENSOR;
	arcan_video_alterfeed(mvctx->vid, FFUNC_VR,
		(struct vfunc_state){
		.tag = ARCAN_TAG_VR,
		.ptr = vrctx
	});

/* nothing more we need to preroll so activate immediately */
	platform_fsrv_pushevent(mvctx,
		&(arcan_event){
			.category = EVENT_TARGET,
			.tgt.kind = TARGET_COMMAND_ACTIVATE
	});

	return vrctx;
}

/*
 * This ffunc is a simplified version of the arcan_frameserver_emptyframe
 * where we also check limb updates and synch position / head tracking
 */
enum arcan_ffunc_rv arcan_vr_ffunc FFUNC_HEAD
{
	struct arcan_vr_ctx* ctx = state.ptr;
	struct arcan_frameserver* tgt = ctx->connection;

	if (!tgt || state.tag != ARCAN_TAG_VR)
		return FRV_NOFRAME;

/* poll allocation mask for events */
	TRAMP_GUARD(FRV_NOFRAME, tgt);

	if (cmd == FFUNC_DESTROY){
		arcan_frameserver_free(tgt);
		ctx->connection = NULL;

		for (size_t i = 0; i < LIMB_LIM; i++){
			if (ctx->limb_map[i].map){
				arcan_3d_bindvr(ctx->limb_map[i].map, NULL);
				ctx->limb_map[i].map = 0;
			}
		}
		return FRV_NOFRAME;
	}

/* target has still not requested access to the VR subprotocol */
	struct arcan_shmif_vr* vr = tgt->desc.aext.vr;
	if (!vr){
		if (cmd == FFUNC_TICK || (cmd == FFUNC_POLL && tgt->shm.ptr->resized)){
			arcan_frameserver_tick_control(tgt, arcan_event_defaultctx(), FFUNC_VR);
		}
		return FRV_NOFRAME;
	}

	if (cmd == FFUNC_POLL){
		for (size_t i = 0; i < LIMB_LIM; i++){
			if (!ctx->limb_map[i].map)
				continue;

/* see if there is a new sample */
			uint32_t ts = atomic_load(&vr->limbs[i].timestamp);
			if (ts == ctx->limb_map[i].ts)
				continue;

			debug_print("limb %zu updated - %"PRIu32"\n", i, ts);
			struct vr_limb vl = vr->limbs[i];

/* there is, and it verified (failure assumes it is being updated) so using the
 * values is a disservice - the best way is probably to add it to a processing
 * queue and try/flush that queue before leaving */
			uint16_t cs = subp_checksum((uint8_t*)&vl, sizeof(struct vr_limb)-2);
			if (cs != atomic_load(&vr->limbs[i].data.checksum)){
				debug_print("limb %zu failed to validate\n", i);
				continue;
			}

			arcan_vobject* vobj = arcan_video_getobject(ctx->limb_map[i].map);
			assert(vobj);
			vector tb = angle_quat(vl.data.orientation);
			vobj->current.rotation.roll = tb.x;
			vobj->current.rotation.pitch = tb.y;
			vobj->current.rotation.yaw = tb.z;
			vobj->current.rotation.quaternion = vl.data.orientation;
			vobj->current.position = vl.data.position;
		}
	}
	else if (cmd == FFUNC_TICK){
/* check allocation masks */
		uint_least64_t map = atomic_load(&vr->limb_mask);
		uint64_t new = map & ~ctx->map;
		uint64_t lost = ctx->map & ~map;

		if (new){
			debug_print("new map: %"PRIu64, new);
			for (uint64_t i = 0; i < LIMB_LIM; i++){
				if (((uint64_t)1 << i) & new){
					debug_print("added limb (%d)", i);
					arcan_event_enqueue(arcan_event_defaultctx(),
					&(struct arcan_event){
						.category = EVENT_FSRV,
						.fsrv.kind = EVENT_FSRV_ADDVRLIMB,
						.fsrv.limb = i,
						.fsrv.video = tgt->vid,
						.fsrv.otag = tgt->tag
					});
					vr->limbs[i].ignored = true;
				}
			}
		}

		if (lost){
			for (uint64_t i = 0; i < LIMB_LIM; i++){
				if (((uint64_t)1 << i) & lost){
					debug_print("lost limb (%d)", 1 << i);
					arcan_event_enqueue(arcan_event_defaultctx(),
					&(struct arcan_event){
						.category = EVENT_FSRV,
						.fsrv.kind = EVENT_FSRV_LOSTVRLIMB,
						.fsrv.limb = i,
						.fsrv.video = tgt->vid,
						.fsrv.otag = tgt->tag
					});
					if (ctx->limb_map[i].map)
						arcan_3d_bindvr(ctx->limb_map[i].map, NULL);
				}
			}
		}
		ctx->map = map;
	}

/* Run the null-feed as a default handler */
	struct vfunc_state inst = {
		.ptr = tgt,
		.tag = ARCAN_TAG_FRAMESERV
	};

	platform_fsrv_leave();
	return FRV_NOFRAME;
}

arcan_errc arcan_vr_maplimb(
	struct arcan_vr_ctx* ctx, unsigned ind, arcan_vobj_id vid)
{
	if (!ctx || !ctx->connection)
		return ARCAN_ERRC_UNACCEPTED_STATE;

	if (ind >= LIMB_LIM)
		return ARCAN_ERRC_OUT_OF_SPACE;

/* only 1:1 allowed */
	for (size_t i = 0; i < LIMB_LIM; i++)
		if (ctx->limb_map[i].map == vid)
			return ARCAN_ERRC_UNACCEPTED_STATE;

/* unmap- pre-existing? */
	if (ctx->limb_map[ind].map)
		arcan_3d_bindvr(ctx->limb_map[ind].map, NULL);

	ctx->limb_map[ind].map = vid;

	struct arcan_shmif_vr* vr = ctx->connection->desc.aext.vr;
	if (!vr){
		debug_print("trying to limb-map on client without subproto");
		return ARCAN_ERRC_UNACCEPTED_STATE;
	}

/* enable sampling */
	TRAMP_GUARD(ARCAN_ERRC_UNACCEPTED_STATE, ctx->connection);
		vr->limbs[ind].ignored = false;
	platform_fsrv_leave();

	return arcan_3d_bindvr(vid, ctx);
}

arcan_errc arcan_vr_release(struct arcan_vr_ctx* ctx, arcan_vobj_id vid)
{
	arcan_errc rv = ARCAN_ERRC_NO_SUCH_OBJECT;
	for (size_t i = 0; i < LIMB_LIM; i++)
		if (ctx->limb_map[i].map == vid){
			ctx->limb_map[i].map = 0;
			rv = ARCAN_OK;
			TRAMP_GUARD(ARCAN_ERRC_UNACCEPTED_STATE, ctx->connection);
				struct arcan_shmif_vr* vr = ctx->connection->desc.aext.vr;
				vr->limbs[i].ignored = true;
			platform_fsrv_leave();
			break;
		}
	return rv;
}

arcan_errc arcan_vr_displaydata(
	struct arcan_vr_ctx* ctx, struct vr_meta* dst)
{
	struct arcan_frameserver* tgt = ctx->connection;

	if (!tgt || !tgt->desc.aext.vr)
		return ARCAN_ERRC_NO_SUCH_OBJECT;

	struct arcan_shmif_vr* vr = tgt->desc.aext.vr;
	TRAMP_GUARD(ARCAN_ERRC_UNACCEPTED_STATE, tgt);
		*dst = vr->meta;
	platform_fsrv_leave();
	return ARCAN_OK;
}

arcan_errc arcan_vr_shutdown(struct arcan_vr_ctx* ctx)
{
/*
 * enqueue an EXIT_ command on the connection (if alive), enqueue on evctx
 * to indicate that we're gone
 */
	return ARCAN_ERRC_NOT_IMPLEMENTED;
}
