include $(top_srcdir)/example/Makefile.inc

bin_PROGRAMS = odp_classifier$(EXEEXT)
odp_classifier_LDFLAGS = $(AM_LDFLAGS) -static
odp_classifier_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/example

noinst_HEADERS = \
		  $(top_srcdir)/example/example_debug.h

dist_odp_classifier_SOURCES = odp_classifier.c
