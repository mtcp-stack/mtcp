################################################################
# Process this file with top-level configure script to produce Makefile
#
# Copyright 2000 Clark Cooper
#
#  This file is part of EXPAT.
#
#  EXPAT is free software; you can redistribute it and/or modify it
#  under the terms of the License (based on the MIT/X license) contained
#  in the file COPYING that comes with this distribution.
#
# EXPAT IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN EXPAT.
#

SHELL = /bin/bash

srcdir = .
top_srcdir = .


prefix = /usr/local/apache2
exec_prefix = /usr/local/apache2

bindir = /usr/local/apache2/bin
libdir = /usr/local/apache2/lib
includedir = /usr/local/apache2/include
mandir = ${prefix}/man/man1

top_builddir = .


INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
mkinstalldirs = $(SHELL) $(top_srcdir)/conftools/mkinstalldirs

MANFILE = $(srcdir)/doc/xmlwf.1
APIHEADER = $(srcdir)/lib/expat.h
LIBRARY = libexpat.la

CC = gcc
LIBTOOL = $(SHELL) $(top_builddir)/libtool

INCLUDES = -I$(srcdir)/lib -I.
LDFLAGS = 
CPPFLAGS = 
CFLAGS = -g -O2 -DHAVE_EXPAT_CONFIG_H
VSNFLAG = -version-info 5:0:5

### autoconf this?
LTFLAGS = --silent

COMPILE = $(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $(INCLUDES)
LTCOMPILE = $(LIBTOOL) $(LTFLAGS) --mode=compile $(COMPILE)
LINK_LIB = $(LIBTOOL) $(LTFLAGS) --mode=link $(COMPILE) -no-undefined $(VSNFLAG) -rpath $(libdir) $(LDFLAGS) -o $@
LINK_EXE = $(LIBTOOL) $(LTFLAGS) --mode=link $(COMPILE) $(LDFLAGS) -o $@

LIB_OBJS = lib/xmlparse.lo lib/xmltok.lo lib/xmlrole.lo

all: $(LIBRARY)

clean:
	cd lib && rm -f $(LIBRARY) *.o *.lo && rm -rf .libs _libs
	rm -rf .libs libexpat.la

clobber: clean

distclean: clean
	rm -f expat_config.h config.status config.log config.cache libtool
	rm -f Makefile

extraclean: distclean
	rm -f expat_config.h.in configure
	rm -f conftools/aclocal.m4 conftools/ltconfig conftools/ltmain.sh
	rm -f conftools/argz.m4 conftools/libtool.m4 conftools/ltoptions.m4
	rm -f conftools/ltsugar.m4 conftools/ltversion.m4 conftools/lt~obsolete.m4

check: tests/runtests
	tests/runtests

install: installlib

installlib: $(LIBRARY) $(APIHEADER)
	$(mkinstalldirs) $(DESTDIR)$(libdir) $(DESTDIR)$(includedir)
	$(LIBTOOL)  --mode=install $(INSTALL) $(LIBRARY) $(DESTDIR)$(libdir)/$(LIBRARY)
	$(INSTALL_DATA) $(APIHEADER) $(DESTDIR)$(includedir)

$(LIBRARY): $(LIB_OBJS)
	$(LINK_LIB) $(LIB_OBJS)

lib/xmlparse.lo: lib/xmlparse.c lib/expat.h lib/xmlrole.h lib/xmltok.h \
	$(top_builddir)/expat_config.h lib/internal.h

lib/xmltok.lo: lib/xmltok.c lib/xmltok_impl.c lib/xmltok_ns.c \
	lib/ascii.h lib/asciitab.h lib/iasciitab.h lib/latin1tab.h \
	lib/nametab.h lib/utf8tab.h lib/xmltok.h lib/xmltok_impl.h \
	$(top_builddir)/expat_config.h

.SUFFIXES: .c .lo .o

.c.o:
	$(COMPILE) -o $@ -c $<
.c.lo:
	$(LTCOMPILE) -o $@ -c $<

.PHONY: buildlib all \
	clean distclean extraclean maintainer-clean \
	dist distdir \
	install uninstall
