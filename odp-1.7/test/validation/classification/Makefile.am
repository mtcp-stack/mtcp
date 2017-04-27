include ../Makefile.inc

noinst_LTLIBRARIES = libtestclassification.la
libtestclassification_la_SOURCES = odp_classification_basic.c \
			       odp_classification_tests.c \
			       odp_classification_test_pmr.c \
			       odp_classification_common.c \
			       classification.c

test_PROGRAMS = classification_main$(EXEEXT)
dist_classification_main_SOURCES = classification_main.c
classification_main_LDADD = libtestclassification.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = classification.h odp_classification_testsuites.h
