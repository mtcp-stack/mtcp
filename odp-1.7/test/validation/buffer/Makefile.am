include ../Makefile.inc

noinst_LTLIBRARIES = libtestbuffer.la
libtestbuffer_la_SOURCES = buffer.c

test_PROGRAMS = buffer_main$(EXEEXT)
dist_buffer_main_SOURCES = buffer_main.c
buffer_main_LDADD = libtestbuffer.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = buffer.h
