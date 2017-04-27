# Uncomment this if you need to change the CUSTOM_STR string
#export CUSTOM_STR=https://git.linaro.org/lng/odp.git

include $(top_srcdir)/platform/Makefile.inc
include $(top_srcdir)/platform/@with_platform@/Makefile.inc

AM_CFLAGS +=  -I$(srcdir)/include
AM_CFLAGS +=  -I$(top_srcdir)/include
AM_CFLAGS +=  -I$(top_srcdir)/helper/include

include_HEADERS = \
		  $(top_srcdir)/include/odp.h

odpincludedir= $(includedir)/odp
odpinclude_HEADERS = \
		  $(srcdir)/include/odp/align.h \
		  $(srcdir)/include/odp/atomic.h \
		  $(srcdir)/include/odp/barrier.h \
		  $(srcdir)/include/odp/buffer.h \
		  $(srcdir)/include/odp/byteorder.h \
		  $(srcdir)/include/odp/classification.h \
		  $(srcdir)/include/odp/compiler.h \
		  $(srcdir)/include/odp/config.h \
		  $(srcdir)/include/odp/cpu.h \
		  $(srcdir)/include/odp/cpumask.h \
		  $(srcdir)/include/odp/crypto.h \
		  $(srcdir)/include/odp/debug.h \
		  $(srcdir)/include/odp/errno.h \
		  $(srcdir)/include/odp/event.h \
		  $(srcdir)/include/odp/hash.h \
		  $(srcdir)/include/odp/hints.h \
		  $(srcdir)/include/odp/init.h \
		  $(srcdir)/include/odp/packet_flags.h \
		  $(srcdir)/include/odp/packet.h \
		  $(srcdir)/include/odp/packet_io.h \
		  $(srcdir)/include/odp/pool.h \
		  $(srcdir)/include/odp/queue.h \
		  $(srcdir)/include/odp/random.h \
		  $(srcdir)/include/odp/rwlock.h \
		  $(srcdir)/include/odp/rwlock_recursive.h \
		  $(srcdir)/include/odp/schedule.h \
		  $(srcdir)/include/odp/schedule_types.h \
		  $(srcdir)/include/odp/shared_memory.h \
		  $(srcdir)/include/odp/spinlock.h \
		  $(srcdir)/include/odp/spinlock_recursive.h \
		  $(srcdir)/include/odp/std_clib.h \
		  $(srcdir)/include/odp/std_types.h \
		  $(srcdir)/include/odp/sync.h \
		  $(srcdir)/include/odp/system_info.h \
		  $(srcdir)/include/odp/thread.h \
		  $(srcdir)/include/odp/thrmask.h \
		  $(srcdir)/include/odp/ticketlock.h \
		  $(srcdir)/include/odp/time.h \
		  $(srcdir)/include/odp/timer.h \
		  $(srcdir)/include/odp/version.h \
		  $(srcdir)/arch/@ARCH@/odp/cpu_arch.h

odpplatincludedir= $(includedir)/odp/plat
odpplatinclude_HEADERS = \
		  $(srcdir)/include/odp/plat/atomic_types.h \
		  $(srcdir)/include/odp/plat/barrier_types.h \
		  $(srcdir)/include/odp/plat/buffer_types.h \
		  $(srcdir)/include/odp/plat/byteorder_types.h \
		  $(srcdir)/include/odp/plat/classification_types.h \
		  $(srcdir)/include/odp/plat/cpumask_types.h \
		  $(srcdir)/include/odp/plat/crypto_types.h \
		  $(srcdir)/include/odp/plat/event_types.h \
		  $(srcdir)/include/odp/plat/init_types.h \
		  $(srcdir)/include/odp/plat/packet_types.h \
		  $(srcdir)/include/odp/plat/packet_io_types.h \
		  $(srcdir)/include/odp/plat/pool_types.h \
		  $(srcdir)/include/odp/plat/queue_types.h \
		  $(srcdir)/include/odp/plat/rwlock_types.h \
		  $(srcdir)/include/odp/plat/rwlock_recursive_types.h \
		  $(srcdir)/include/odp/plat/schedule_types.h \
		  $(srcdir)/include/odp/plat/shared_memory_types.h \
		  $(srcdir)/include/odp/plat/spinlock_types.h \
		  $(srcdir)/include/odp/plat/spinlock_recursive_types.h \
		  $(srcdir)/include/odp/plat/strong_types.h \
		  $(srcdir)/include/odp/plat/thread_types.h \
		  $(srcdir)/include/odp/plat/thrmask_types.h \
		  $(srcdir)/include/odp/plat/ticketlock_types.h \
		  $(srcdir)/include/odp/plat/time_types.h \
		  $(srcdir)/include/odp/plat/timer_types.h \
		  $(srcdir)/include/odp/plat/version_types.h

noinst_HEADERS = \
		  ${srcdir}/include/odp_align_internal.h \
		  ${srcdir}/include/odp_atomic_internal.h \
		  ${srcdir}/include/odp_buffer_inlines.h \
		  ${srcdir}/include/odp_buffer_internal.h \
		  ${srcdir}/include/odp_classification_datamodel.h \
		  ${srcdir}/include/odp_classification_inlines.h \
		  ${srcdir}/include/odp_classification_internal.h \
		  ${srcdir}/include/odp_crypto_internal.h \
		  ${srcdir}/include/odp_debug_internal.h \
		  ${srcdir}/include/odp_forward_typedefs_internal.h \
		  ${srcdir}/include/odp_internal.h \
		  ${srcdir}/include/odp_packet_internal.h \
		  ${srcdir}/include/odp_packet_io_internal.h \
		  ${srcdir}/include/odp_packet_io_queue.h \
		  ${srcdir}/include/odp_packet_netmap.h \
		  ${srcdir}/include/odp_packet_socket.h \
		  ${srcdir}/include/odp_packet_tap.h \
		  ${srcdir}/include/odp_pool_internal.h \
		  ${srcdir}/include/odp_posix_extensions.h \
		  ${srcdir}/include/odp_queue_internal.h \
		  ${srcdir}/include/odp_schedule_internal.h \
		  ${srcdir}/include/odp_timer_internal.h \
		  ${srcdir}/Makefile.inc

__LIB__libodp_la_SOURCES = \
			   odp_atomic.c \
			   odp_barrier.c \
			   odp_buffer.c \
			   odp_classification.c \
			   odp_cpu.c \
			   odp_cpumask.c \
			   odp_cpumask_task.c \
			   odp_crypto.c \
			   odp_errno.c \
			   odp_event.c \
			   odp_hash.c \
			   odp_init.c \
			   odp_impl.c \
			   odp_packet.c \
			   odp_packet_flags.c \
			   odp_packet_io.c \
			   pktio/ethtool.c \
			   pktio/io_ops.c \
			   pktio/pktio_common.c \
			   pktio/loop.c \
			   pktio/netmap.c \
			   pktio/socket.c \
			   pktio/socket_mmap.c \
			   pktio/sysfs.c \
			   pktio/tap.c \
			   odp_pool.c \
			   odp_queue.c \
			   odp_rwlock.c \
			   odp_rwlock_recursive.c \
			   odp_schedule.c \
			   odp_shared_memory.c \
			   odp_spinlock.c \
			   odp_spinlock_recursive.c \
			   odp_system_info.c \
			   odp_thread.c \
			   odp_thrmask.c \
			   odp_ticketlock.c \
			   odp_time.c \
			   odp_timer.c \
			   odp_version.c \
			   odp_weak.c \
			   arch/@ARCH@/odp_cpu_arch.c \
			   arch/@ARCH@/odp_sysinfo_parse.c

EXTRA_DIST = \
	     arch/linux/odp_cpu_arch.c \
	     arch/linux/odp_sysinfo_parse.c \
	     arch/mips64/odp_cpu_arch.c \
	     arch/mips64/odp_sysinfo_parse.c \
	     arch/powerpc/odp_cpu_arch.c \
	     arch/powerpc/odp_sysinfo_parse.c \
	     arch/x86/odp_cpu_arch.c \
	     arch/x86/odp_sysinfo_parse.c

if HAVE_PCAP
__LIB__libodp_la_SOURCES += pktio/pcap.c
endif
