# Stuff for main library building
C_FILES = \
		cidr_addr.c \
		cidr_compare.c \
		cidr_from_str.c \
		cidr_get.c \
		cidr_inaddr.c \
		cidr_mem.c \
		cidr_misc.c \
		cidr_net.c \
		cidr_num.c \
		cidr_to_str.c
CLEAN_FILES = ${SO_FILES} ${SHLIB_NAME} ${SHLIB_LINK} ${STATICLIB_NAME} ${STATICLIB_LINK} *core .depend *.o *.So

.include "Makefile.inc"

# This need to be after the include
all build: ${SHLIB_NAME} ${STATICLIB_NAME}

