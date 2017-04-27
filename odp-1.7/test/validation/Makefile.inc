include $(top_srcdir)/test/Makefile.inc

COMMON_DIR = $(top_builddir)/test/validation/common

#the following option ensure that option '-I.' is not passed to gcc,
#therefore distinguishing between '#include "X"' and '#include <X>'.
#It allows common filenames (such as 'errno.h') to be used locally.
AUTOMAKE_OPTIONS = nostdinc

AM_CFLAGS += -I$(top_srcdir)/test/validation/common
AM_LDFLAGS += -static

LIBCUNIT_COMMON = $(COMMON_DIR)/libcunit_common.la
LIBCPUMASK_COMMON = $(COMMON_DIR)/libcpumask_common.la
LIBTHRMASK_COMMON = $(COMMON_DIR)/libthrmask_common.la
LIBODP = $(LIB)/libodphelper.la $(LIB)/libodp.la
