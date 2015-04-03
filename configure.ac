AC_INIT(config, version-0.1)

# Check for cc
AC_PROG_CC
AC_LANG(C)
AC_DISABLE_OPTION_CHECKING

# Check for scheduling
AC_CHECK_HEADER(linux/sched.h,,,) 
# Check for pthreads
AC_CHECK_HEADER(pthread.h,,,)
# Check for numa 
AC_CHECK_HEADER(numa.h,,,)
# Reset DPDK to 0
AC_SUBST(DPDK, 0)
# Reset PSIO to 0
AC_SUBST(PSIO, 0)

# Check dpdk-1.8.0 path (lib & inc)
AC_ARG_WITH(stuff, [  --with-dpdk-lib      path to the dpdk-1.8.0 install root])
if test "$with_dpdk_lib" != ""
then
        AC_SUBST(DPDKLIBPATH, $with_dpdk_lib)
	AC_SUBST(DPDK, 1)
fi

# Check psio path (lib & inc)
AC_ARG_WITH(stuff, [  --with-psio-lib      path to the ioengine install root])
if test "$with_psio_lib" != ""
then
        AC_SUBST(PSLIBPATH, $with_psio_lib)
	AC_SUBST(PSIO, 1)
fi

if test "$with_psio_lib" == "" && test "$with_dpdk_lib" == ""
then
	AC_MSG_ERROR([Packet I/O library is missing. Please set either dpdk or psio as your I/O lib.])
fi

AC_OUTPUT(mtcp/src/Makefile)
AC_OUTPUT(apps/example/Makefile)

