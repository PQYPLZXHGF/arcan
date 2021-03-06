seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(access), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(open), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(stat), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(arch_prctl), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(set_tid_address), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(set_robust_list), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigaction), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigprocmask), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(getrlimit), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(epoll_create1), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(socket), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(connect), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(statfs), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(unlink), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(recvmsg), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(geteuid), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(getegid), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(getuid), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(getgid), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(readlink), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(getdents), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(mincore), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(flock), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(bind), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(listen), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(epoll_ctl), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(poll), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(epoll_wait), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(accept4), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(getsockopt), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(sendmsg), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(mremap), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(clone), 0);
seccomp_rule_add(flt, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
