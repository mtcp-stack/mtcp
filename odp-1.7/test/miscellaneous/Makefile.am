include $(top_srcdir)/test/Makefile.inc

if test_cpp
bin_PROGRAMS =  odp_api_from_cpp$(EXEEXT)
TESTS = odp_api_from_cpp$(EXEEXT)
endif

odp_api_from_cpp_CXXFLAGS = $(AM_CXXFLAGS)

odp_api_from_cpp_LDFLAGS = $(AM_LDFLAGS) -static

dist_odp_api_from_cpp_SOURCES = odp_api_from_cpp.cpp
