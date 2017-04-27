LIB   = $(top_builddir)/lib

dist_pkgdata_DATA = $(LIB)/libodp.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(top_builddir)/pkgconfig/libodp.pc

.PHONY: pkgconfig/libodp.pc

VPATH = $(srcdir) $(builddir)
lib_LTLIBRARIES = $(LIB)/libodp.la

AM_LDFLAGS += -version-number '$(ODP_LIBSO_VERSION)'

GIT_DESC = `$(top_srcdir)/scripts/get_impl_str.sh $(top_srcdir)`
AM_CFLAGS += "-DGIT_HASH=$(GIT_DESC)"
AM_CFLAGS += -DPLATFORM=${with_platform}

#The implementation will need to retain the deprecated implementation
AM_CFLAGS += -Wno-deprecated-declarations

odpapiincludedir= $(includedir)/odp/api
odpapiinclude_HEADERS = \
		  $(top_srcdir)/include/odp/api/align.h \
		  $(top_srcdir)/include/odp/api/atomic.h \
		  $(top_srcdir)/include/odp/api/barrier.h \
		  $(top_srcdir)/include/odp/api/buffer.h \
		  $(top_srcdir)/include/odp/api/byteorder.h \
		  $(top_srcdir)/include/odp/api/classification.h \
		  $(top_srcdir)/include/odp/api/compiler.h \
		  $(top_srcdir)/include/odp/api/config.h \
		  $(top_srcdir)/include/odp/api/cpu.h \
		  $(top_srcdir)/include/odp/api/cpumask.h \
		  $(top_srcdir)/include/odp/api/crypto.h \
		  $(top_srcdir)/include/odp/api/debug.h \
		  $(top_srcdir)/include/odp/api/errno.h \
		  $(top_srcdir)/include/odp/api/event.h \
		  $(top_srcdir)/include/odp/api/hash.h \
		  $(top_srcdir)/include/odp/api/hints.h \
		  $(top_srcdir)/include/odp/api/init.h \
		  $(top_srcdir)/include/odp/api/packet.h \
		  $(top_srcdir)/include/odp/api/packet_flags.h \
		  $(top_srcdir)/include/odp/api/packet_io.h \
		  $(top_srcdir)/include/odp/api/packet_io_stats.h \
		  $(top_srcdir)/include/odp/api/pool.h \
		  $(top_srcdir)/include/odp/api/queue.h \
		  $(top_srcdir)/include/odp/api/random.h \
		  $(top_srcdir)/include/odp/api/rwlock.h \
		  $(top_srcdir)/include/odp/api/rwlock_recursive.h \
		  $(top_srcdir)/include/odp/api/schedule.h \
		  $(top_srcdir)/include/odp/api/schedule_types.h \
		  $(top_srcdir)/include/odp/api/shared_memory.h \
		  $(top_srcdir)/include/odp/api/spinlock.h \
		  $(top_srcdir)/include/odp/api/spinlock_recursive.h \
		  $(top_srcdir)/include/odp/api/std_clib.h \
		  $(top_srcdir)/include/odp/api/std_types.h \
		  $(top_srcdir)/include/odp/api/sync.h \
		  $(top_srcdir)/include/odp/api/system_info.h \
		  $(top_srcdir)/include/odp/api/thread.h \
		  $(top_srcdir)/include/odp/api/thrmask.h \
		  $(top_srcdir)/include/odp/api/ticketlock.h \
		  $(top_srcdir)/include/odp/api/time.h \
		  $(top_srcdir)/include/odp/api/timer.h \
		  $(top_srcdir)/include/odp/api/version.h
