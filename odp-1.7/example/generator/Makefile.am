include $(top_srcdir)/example/Makefile.inc

bin_PROGRAMS = odp_generator$(EXEEXT)
odp_generator_LDFLAGS = $(AM_LDFLAGS) -static
odp_generator_CFLAGS = $(AM_CFLAGS) -I${top_srcdir}/example

noinst_HEADERS = \
		  $(top_srcdir)/example/example_debug.h

dist_odp_generator_SOURCES = odp_generator.c
