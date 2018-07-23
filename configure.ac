#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mtcp], [3], [mtcp-user@list.ndsl.kaist.edu])
AC_CONFIG_SRCDIR([mtcp/src/eventpoll.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AC_LANG(C)
AC_DISABLE_OPTION_CHECKING
AC_CHECK_PROG(BC_CHECK,bc,yes)
if test x"$BC_CHECK" != x"yes" ; then
    AC_MSG_ERROR([Please install bc before compiling mTCP.])
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER([linux/sched.h],,AC_MSG_ERROR([Could not find linux/sched.h]))
AC_CHECK_HEADER([pthread.h],,AC_MSG_ERROR([Could not find pthread.h]))
AC_CHECK_HEADER([numa.h],,AC_MSG_ERROR([Could not find numa.h]))
AC_CHECK_HEADER([arpa/inet.h],,AC_MSG_ERROR([Could not find arpa/inet.h]))
AC_CHECK_HEADER([fcntl.h],,AC_MSG_ERROR([Could not find fcntl.h]))
AC_CHECK_HEADER([limits.h],,AC_MSG_ERROR([Could not find limits.h]))
AC_CHECK_HEADER([netdb.h],,AC_MSG_ERROR([netdb.h]))
AC_CHECK_HEADER([netinet/in.h],,AC_MSG_ERROR([Could not find netinet/in.h]))
AC_CHECK_HEADER([stdint.h],,AC_MSG_ERROR([Could not find stdint.h]))
AC_CHECK_HEADER([stdlib.h],,AC_MSG_ERROR([Could not find stdlib.h]))
AC_CHECK_HEADER([string.h],,AC_MSG_ERROR([Could not find string.h]))
AC_CHECK_HEADER([sys/ioctl.h],,AC_MSG_ERROR([Could not find sys/ioctl.h]))
AC_CHECK_HEADER([sys/socket.h],,AC_MSG_ERROR([Could not find sys/socket.h]))
AC_CHECK_HEADER([sys/time.h],,AC_MSG_ERROR([Could not find sys/time.h]))
AC_CHECK_HEADER([unistd.h],,AC_MSG_ERROR([Could not find unistd.h]))
AC_CHECK_HEADER([gmp.h],,AC_MSG_ERROR([Could not find gmp.h]))

# Check for AC_PTHREAD [FC is preventing this to be enabled..]
#AX_PTHREAD( [
#            AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.])
#            CLIBS="$PTHREAD_LIBS $LIBS"
#            CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
#            LDFLAGS="$LDFLAGS $PTHREAD_CFLAGS"
#            CC="$PTHREAD_CC"],[])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNC([clock_gettime],,AC_MSG_ERROR([librt library is missing]))
AC_CHECK_FUNCS([bzero getpagesize gettimeofday memmove memset munmap select socket strchr strerror strstr strtol],,AC_MSG_ERROR([glibc library is missing]))

# Reset DPDK to 0
AC_SUBST(DPDK, 0)
# Reset enforcement value
AC_SUBST(ENFORCE_RX_IDLE, 0)
AC_SUBST(RX_IDLE_THRESH, 0)
# Reset DPDK's LRO to 0
AC_SUBST(LRO, 0)
# Reset PSIO to 0
AC_SUBST(PSIO, 0)
# Reset ONVM to 0
AC_SUBST(ONVM, 0)
# Reset NETMAP to 0
AC_SUBST(NETMAP, 0)
# Reset HWCSUM to 1
AC_SUBST(HWCSUM, 1)

# Check dpdk-17.08 path (lib & inc)
AC_ARG_WITH(stuff, [  --with-dpdk-lib      path to the dpdk-17.08 install root])
if test "$with_dpdk_lib" != ""
then
        AC_SUBST(DPDKLIBPATH, $with_dpdk_lib)
	AC_SUBST(DPDK, 1)
	AC_ARG_WITH(stuff, [  --with-rx-idle[=ARG]	how much idle cycles needed before enforcing timeout (dpdk-only)])
	if test "$with_rx_idle" != ""
	then
		AC_SUBST(ENFORCE_RX_IDLE, 1)
		AC_SUBST(RX_IDLE_THRESH, $with_rx_idle)
	fi
fi
AM_CONDITIONAL([DPDK], [test x$DPDK = x1])

dnl Example of default-disabled feature
AC_ARG_ENABLE([lro],
	AS_HELP_STRING([--enable-lro], [Enable dpdk lro (for relevant NICs)]))

if test "$with_dpdk_lib" != ""
then
	if test "x$enable_lro" = "xyes"
	then
		AC_SUBST(LRO, 1)
	fi
fi

dnl Example of default-enabled feature
AC_ARG_ENABLE([hwcsum],
	AS_HELP_STRING([--disable-hwcsum], [Disable h/w-based checksum offloading (for relevant NICs)]))

if test "x$enable_hwcsum" = "xno"
then
	AC_SUBST(HWCSUM, 0)
fi

# Check psio path (lib & inc)
AC_ARG_WITH(stuff, [  --with-psio-lib      path to the ioengine install root])
if test "$with_psio_lib" != ""
then
        AC_SUBST(PSLIBPATH, $with_psio_lib)
	AC_SUBST(PSIO, 1)
fi

dnl Example of default-disabled feature
AC_ARG_ENABLE([netmap],
	AS_HELP_STRING([--enable-netmap], [Enable netmap module]))

AS_IF([test "x$enable_netmap" = "xyes"], [
	    AC_SUBST(NETMAP, 1)
])

# Check onvm lib path
AC_ARG_WITH(stuff, [  --with-onvm-lib      path to the onvm install root])
if test "$with_onvm_lib" != ""
then
	AC_SUBST(ONVMLIBPATH, $with_onvm_lib)
	AC_SUBST(ONVM, 1)
fi

if test "$with_psio_lib" == "" && test "$with_dpdk_lib" == "" && test "$enable_netmap" = ""
then
	AC_MSG_ERROR([Packet I/O library is missing. Please set either dpdk or psio or netmap as your I/O lib.])
fi

AM_INIT_AUTOMAKE(mtcp, 3)
AC_CONFIG_FILES([Makefile])
AC_OUTPUT(mtcp/src/Makefile)
AC_OUTPUT(apps/example/Makefile)
AC_OUTPUT(util/Makefile)
AC_OUTPUT(io_engine/lib/Makefile)

bash ./.check_hyperthreading.sh
