include ../Makefile.inc

noinst_LTLIBRARIES = libtestcrypto.la
libtestcrypto_la_SOURCES = crypto.c \
		       odp_crypto_test_inp.c

test_PROGRAMS = crypto_main$(EXEEXT)
dist_crypto_main_SOURCES = crypto_main.c
crypto_main_LDADD = libtestcrypto.la $(LIBCUNIT_COMMON) $(LIBODP)

EXTRA_DIST = crypto.h odp_crypto_test_inp.h test_vectors.h test_vectors_len.h
