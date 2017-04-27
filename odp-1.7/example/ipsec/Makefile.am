include $(top_srcdir)/example/Makefile.inc

bin_PROGRAMS = odp_ipsec$(EXEEXT)
odp_ipsec_LDFLAGS = $(AM_LDFLAGS) -static
odp_ipsec_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/example

noinst_HEADERS = \
		  $(top_srcdir)/example/ipsec/odp_ipsec_cache.h \
		  $(top_srcdir)/example/ipsec/odp_ipsec_fwd_db.h \
		  $(top_srcdir)/example/ipsec/odp_ipsec_loop_db.h \
		  $(top_srcdir)/example/ipsec/odp_ipsec_misc.h \
		  $(top_srcdir)/example/ipsec/odp_ipsec_sa_db.h \
		  $(top_srcdir)/example/ipsec/odp_ipsec_sp_db.h \
		  $(top_srcdir)/example/ipsec/odp_ipsec_stream.h \
		  $(top_srcdir)/example/example_debug.h

dist_bin_SCRIPTS = \
		   $(srcdir)/run_ah_in \
		   $(srcdir)/run_ah_out \
		   $(srcdir)/run_both_in \
		   $(srcdir)/run_both_out \
		   $(srcdir)/run_esp_in \
		   $(srcdir)/run_esp_out \
		   $(srcdir)/run_live \
		   $(srcdir)/run_router \
		   $(srcdir)/run_simple

dist_odp_ipsec_SOURCES = odp_ipsec.c \
			 odp_ipsec_sa_db.c \
			 odp_ipsec_sp_db.c \
			 odp_ipsec_fwd_db.c \
			 odp_ipsec_loop_db.c \
			 odp_ipsec_cache.c \
			 odp_ipsec_stream.c
