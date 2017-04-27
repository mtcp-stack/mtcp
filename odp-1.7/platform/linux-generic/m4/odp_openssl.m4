##########################################################################
# Set optional OpenSSL path
##########################################################################
AC_ARG_WITH([openssl-path],
AC_HELP_STRING([--with-openssl-path=DIR path to openssl libs and headers],
               [(or in the default path if not specified).]),
    [OPENSSL_PATH=$withval
    AM_CPPFLAGS="$AM_CPPFLAGS -I$OPENSSL_PATH/include"
    AM_LDFLAGS="$AM_LDFLAGS -L$OPENSSL_PATH/lib"
    ],[])

##########################################################################
# Save and set temporary compilation flags
##########################################################################
OLD_LDFLAGS=$LDFLAGS
OLD_CPPFLAGS=$CPPFLAGS
LDFLAGS="$AM_LDFLAGS $LDFLAGS"
CPPFLAGS="$AM_CPPFLAGS $CPPFLAGS"

##########################################################################
# Check for OpenSSL availability
##########################################################################
AC_CHECK_LIB([crypto], [EVP_EncryptInit], [],
             [AC_MSG_FAILURE([OpenSSL libraries required])])
AC_CHECK_HEADERS([openssl/des.h openssl/rand.h openssl/hmac.h openssl/evp.h], [],
             [AC_MSG_ERROR([OpenSSL headers required])])

##########################################################################
# Restore old saved variables
##########################################################################
LDFLAGS=$OLD_LDFLAGS
CPPFLAGS=$OLD_CPPFLAGS
