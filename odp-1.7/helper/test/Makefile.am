include $(top_srcdir)/test/Makefile.inc

AM_CFLAGS += -I$(srcdir)/common
AM_LDFLAGS += -static

TESTS_ENVIRONMENT += TEST_DIR=${builddir}

EXECUTABLES = odp_chksum$(EXEEXT) \
              odp_thread$(EXEEXT) \
              odp_process$(EXEEXT)\
              odp_table$(EXEEXT)

COMPILE_ONLY =

TESTSCRIPTS =

if test_helper
TESTS = $(EXECUTABLES) $(TESTSCRIPTS)
endif

dist_bin_SCRIPTS =

bin_PROGRAMS = $(EXECUTABLES) $(COMPILE_ONLY)


dist_odp_chksum_SOURCES = odp_chksum.c
dist_odp_thread_SOURCES = odp_thread.c
odp_thread_LDADD = $(LIB)/libodphelper.la $(LIB)/libodp.la
dist_odp_process_SOURCES = odp_process.c
odp_process_LDADD = $(LIB)/libodphelper.la $(LIB)/libodp.la
dist_odp_table_SOURCES = odp_table.c
