AC_PREREQ([2.5])
AC_INIT([OpenDataPlane], m4_esyscmd(./scripts/git_hash.sh .), [lng-odp@lists.linaro.org])
AM_INIT_AUTOMAKE([1.9 tar-pax subdir-objects])
AC_CONFIG_SRCDIR([helper/config.h.in])
AM_CONFIG_HEADER([helper/config.h])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_CXX

AC_PROG_INSTALL
AC_PROG_MAKE_SET

AM_PROG_AR
#Use libtool
LT_INIT([])
AC_SUBST([LIBTOOL_DEPS])
AM_PROG_LIBTOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero clock_gettime gethostbyname getpagesize gettimeofday memset munmap socket strchr strerror strrchr strstr strtoull])

# Checks for header files.
AC_HEADER_RESOLV
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

##########################################################################
# Allow valgrind suite to run against the defined tests
##########################################################################
AX_VALGRIND_CHECK

##########################################################################
# Which architecture optimizations will we use
##########################################################################
AS_CASE([$host],
  [x86*], [ARCH=x86],
  [mips64*], [ARCH=mips64],
  [powerpc*], [ARCH=powerpc],
  [ARCH=linux]
)
AC_SUBST([ARCH])

##########################################################################
# Set correct platform library version
##########################################################################
AGE=0
ODP_LIBSO_VERSION=$(echo $VERSION.$AGE | awk -F . '{printf("%d:%d:%d\n", (($1 * 100) + $2), $3, $4)}')
AC_SUBST(ODP_LIBSO_VERSION)

##########################################################################
# Determine which platform to build for
##########################################################################
AC_ARG_WITH([platform],
    [AS_HELP_STRING([--with-platform=platform],
        [select platform to be used, default linux-generic])],
    [],
    [with_platform=linux-generic
    ])

AC_SUBST([with_platform])
AC_SUBST([platform_with_platform], ["platform/${with_platform}"])
AC_SUBST([platform_with_platform_test], ["platform/${with_platform}/test"])

##########################################################################
# Prepare default values for platform specific optional features
##########################################################################
netmap_support=no

##########################################################################
# Run platform specific checks and settings
##########################################################################
if test "${with_platform}" == "linux-generic";
then
    m4_include([./platform/linux-generic/m4/configure.m4])
else
    echo "UNSUPPORTED PLATFORM: ${with_platform}"
    exit 1
fi

##########################################################################
# Set conditionals as computed within platform specific files
##########################################################################
AM_CONDITIONAL([netmap_support], [test x$netmap_support = xyes ])
AM_CONDITIONAL([HAVE_PCAP], [test $have_pcap = yes])


AC_ARG_WITH([sdk-install-path],
AC_HELP_STRING([--with-sdk-install-path=DIR path to external libs and headers],
             [(or in the default path if not specified).]),
[SDK_INSTALL_PATH=$withval SDK_INSTALL_PATH_=1],[SDK_INSTALL_PATH_=])

AC_SUBST(SDK_INSTALL_PATH)

AM_CONDITIONAL([SDK_INSTALL_PATH_], [test "x${SDK_INSTALL_PATH_}" = "x1"])

##########################################################################
# Set the install directory for test binaries/scripts
##########################################################################
AC_ARG_WITH([testdir],
    AC_HELP_STRING([--with-testdir=DIR installation directory for tests]),
        [if test "$withval" = "yes"; then
             testdir=$libdir/odp/tests
         else
             testdir=$withval
         fi], [])
AC_SUBST([testdir])
AM_CONDITIONAL([test_installdir], [test "$testdir" != ""])

##########################################################################
# Enable/disable Unit tests
##########################################################################
cunit_support=no
AC_ARG_ENABLE([cunit_support],
    [  --enable-cunit-support  include cunit infrastructure],
    [if test x$enableval = xyes; then
        cunit_support=yes
    fi])


##########################################################################
# Setup doxygen documentation
##########################################################################
DX_HTML_FEATURE(ON)
DX_PDF_FEATURE(ON)
DX_INIT_DOXYGEN($PACKAGE_NAME, doc/application-api-guide/doxygen.cfg, doc/output)
##########################################################################
# Enable/disable Unit tests
##########################################################################
test_vald=no
AC_ARG_ENABLE([test_vald],
    [  --enable-test-vald       run test in test/validation],
    [if test x$enableval = xyes; then
        test_vald=yes
        cunit_support=yes
    fi])

AM_CONDITIONAL([test_vald], [test x$test_vald = xyes ])

##########################################################################
# Enable/disable test-perf
##########################################################################
test_perf=no
AC_ARG_ENABLE([test-perf],
    [  --enable-test-perf      run test in test/performance],
    [if test "x$enableval" = "xyes"; then
        test_perf=yes
    fi])

AM_CONDITIONAL([test_perf], [test x$test_perf = xyes ])

##########################################################################
# Enable/disable test-helper
##########################################################################
test_helper=yes
AC_ARG_ENABLE([test-helper],
    [  --enable-test-helper      run test in helper/test],
    [if test "x$enableval" = "xyes"; then
        test_helper=yes
    fi])

AM_CONDITIONAL([test_helper], [test x$test_helper = xyes ])

##########################################################################
# Enable/disable test-cpp
##########################################################################
test_cpp=no
AC_ARG_ENABLE([test-cpp],
    [  --enable-test-cpp       run basic test aginast cpp],
    [if test "x$enableval" = "xyes"; then
        test_cpp=yes
     else
        test_cpp=no
    fi])

AM_CONDITIONAL([test_cpp], [test x$test_cpp = xyes ])

##########################################################################
# Set optional CUnit path
##########################################################################
AC_ARG_WITH([cunit-path],
AC_HELP_STRING([--with-cunit-path=DIR   path to CUnit libs and headers],
               [(or in the default path if not specified).]),
    [CUNIT_PATH=$withval
    AM_CPPFLAGS="$AM_CPPFLAGS -I$CUNIT_PATH/include"
    AM_LDFLAGS="$AM_LDFLAGS -L$CUNIT_PATH/lib"
    cunit_support=yes],[])

##########################################################################
# Enable/disable ODP_DEBUG_PRINT
##########################################################################
ODP_DEBUG_PRINT=1
AC_ARG_ENABLE([debug-print],
    [  --enable-debug-print    display debugging information],
    [if ! test "x$enableval" = "xyes"; then
        ODP_DEBUG_PRINT=0
    fi])
ODP_CFLAGS="$ODP_CFLAGS -DODP_DEBUG_PRINT=$ODP_DEBUG_PRINT"

##########################################################################
# Enable/disable ODP_DEBUG
##########################################################################
ODP_DEBUG=1
AC_ARG_ENABLE([debug],
    [  --enable-debug          include additional code],
    [if ! test "x$enableval" = "xyes"; then
        ODP_DEBUG=0
    fi])
ODP_CFLAGS="$ODP_CFLAGS -DODP_DEBUG=$ODP_DEBUG"

##########################################################################
# Check for doxygen availability
##########################################################################
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN";
   then AC_MSG_WARN([Doxygen not found - continuing without Doxygen support])
fi

AM_CONDITIONAL([HAVE_DOXYGEN], [test "x${DOXYGEN}" = "xdoxygen"])

##########################################################################
# Check for asciidoc availability
##########################################################################
AC_CHECK_PROGS([ASCIIDOC], [asciidoc])
if test -z "$ASCIIDOC";
   then AC_MSG_WARN([asciidoc not found - continuing without asciidoc support])
fi

##########################################################################
# Enable/disable user guide generation
##########################################################################
user_guides=no
AC_ARG_ENABLE([user-guides],
    [  --enable-user-guides    generate supplemental users guides],
    [if test "x$enableval" = "xyes"; then
        if test -z "$ASCIIDOC";
           then AC_MSG_ERROR([cannot generate user guides without asciidoc])
        else
           user_guides=yes
        fi
    fi])

AM_CONDITIONAL([user_guide], [test "x${user_guides}" = "xyes" ])

##########################################################################
# Check for mscgen availability
##########################################################################
       AC_CHECK_PROGS([MSCGEN], [mscgen])
       if test -z "$MSCGEN";
          then AC_MSG_WARN([mscgen not found - continuing without sequence message support])
       fi

AM_CONDITIONAL([HAVE_MSCGEN], [test "x${MSCGEN}" = "xmscgen"])

##########################################################################
# Save and set temporary compilation flags
##########################################################################
OLD_LDFLAGS=$LDFLAGS
OLD_CPPFLAGS=$CPPFLAGS
LDFLAGS="$AM_LDFLAGS $LDFLAGS"
CPPFLAGS="$AM_CPPFLAGS $CPPFLAGS"

##########################################################################
# Check for CUnit availability
##########################################################################
if test x$cunit_support = xyes
then
    AC_CHECK_LIB([cunit],[CU_get_error], [],
        [AC_MSG_ERROR([CUnit libraries required])])
    AC_CHECK_HEADERS([CUnit/Basic.h], [],
        [AC_MSG_FAILURE(["can't find cunit headers"])])
else
    cunit_support=no
fi

AM_CONDITIONAL([cunit_support], [test x$cunit_support = xyes ])

##########################################################################
# Restore old saved variables
##########################################################################
LDFLAGS=$OLD_LDFLAGS
CPPFLAGS=$OLD_CPPFLAGS

##########################################################################
# Default warning setup
##########################################################################
ODP_CFLAGS="$ODP_CFLAGS -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes"
ODP_CFLAGS="$ODP_CFLAGS -Wmissing-declarations -Wold-style-definition -Wpointer-arith"
ODP_CFLAGS="$ODP_CFLAGS -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral"
ODP_CFLAGS="$ODP_CFLAGS -Wformat-security -Wundef -Wwrite-strings"
ODP_CFLAGS="$ODP_CFLAGS -std=c99"
# Extra flags for example to suppress certain warning types
ODP_CFLAGS="$ODP_CFLAGS $ODP_CFLAGS_EXTRA"

##########################################################################
# Default include setup
##########################################################################
AM_CFLAGS="$AM_CFLAGS $ODP_CFLAGS"
AM_CXXFLAGS="-std=c++11"

AC_CONFIG_FILES([Makefile
		 doc/Makefile
		 doc/implementers-guide/Makefile
		 doc/users-guide/Makefile
		 doc/process-guide/Makefile
		 doc/images/Makefile
		 example/Makefile
		 example/classifier/Makefile
		 example/generator/Makefile
		 example/ipsec/Makefile
		 example/packet/Makefile
		 example/time/Makefile
		 example/timer/Makefile
		 helper/Makefile
		 helper/test/Makefile
		 pkgconfig/libodp.pc
		 pkgconfig/libodphelper.pc
		 scripts/Makefile
		 test/Makefile
		 test/api_test/Makefile
		 test/performance/Makefile
		 test/validation/Makefile
		 test/validation/atomic/Makefile
		 test/validation/barrier/Makefile
		 test/validation/buffer/Makefile
		 test/validation/classification/Makefile
		 test/validation/config/Makefile
		 test/validation/cpumask/Makefile
		 test/validation/common/Makefile
		 test/validation/crypto/Makefile
		 test/validation/errno/Makefile
		 test/validation/hash/Makefile
		 test/validation/init/Makefile
		 test/validation/lock/Makefile
		 test/validation/packet/Makefile
		 test/validation/pktio/Makefile
		 test/validation/pool/Makefile
		 test/validation/queue/Makefile
		 test/validation/random/Makefile
		 test/validation/scheduler/Makefile
		 test/validation/std_clib/Makefile
		 test/validation/thread/Makefile
		 test/validation/time/Makefile
		 test/validation/timer/Makefile
		 test/validation/shmem/Makefile
		 test/validation/system/Makefile
		 test/miscellaneous/Makefile
		 ])

AC_SEARCH_LIBS([timer_create],[rt posix4])

##########################################################################
# distribute the changed variables among the Makefiles

AC_SUBST([LIBS])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([CFLAGS])
AC_SUBST([AM_LDFLAGS])
AC_SUBST([LDFLAGS])
AC_SUBST([EXEEXT])

AC_OUTPUT
AC_MSG_RESULT([
	$PACKAGE $VERSION

	Library version:	${ODP_LIBSO_VERSION}

	arch			${ARCH}
	with_platform:		${with_platform}
	prefix:			${prefix}
	sysconfdir:		${sysconfdir}
	libdir:			${libdir}
	includedir:		${includedir}
	testdir:		${testdir}

	cc:			${CC}
	cppflags:		${CPPFLAGS}
	am_cppflags:		${AM_CPPFLAGS}
	am_cxxflags:		${AM_CXXFLAGS}
	cflags:			${CFLAGS}
	am_cflags:		${AM_CFLAGS}
	ldflags:		${LDFLAGS}
	am_ldflags:		${AM_LDFLAGS}
	libs:			${LIBS}
	cunit:			${cunit_support}
	test_vald:		${test_vald}
	test_perf:		${test_perf}
	test_cpp:		${test_cpp}
	test_helper:		${test_helper}
	user_guides:		${user_guides}
])
