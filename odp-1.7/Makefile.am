ACLOCAL_AMFLAGS=-I m4
AUTOMAKE_OPTIONS = foreign
AM_DISTCHECK_CONFIGURE_FLAGS = --enable-test-cpp \
			       --enable-test-perf \
			       --enable-test-vald \
			       --with-testdir

#@with_platform@ works alone in subdir but not as part of a path???
SUBDIRS = @platform_with_platform@ \
	  helper \
	  test \
	  @platform_with_platform_test@ \
	  helper/test \
	  doc \
	  example \
	  scripts

include $(top_srcdir)/aminclude.am

EXTRA_DIST = bootstrap $(DX_CONFIG) CHANGELOG config/README .scmversion
