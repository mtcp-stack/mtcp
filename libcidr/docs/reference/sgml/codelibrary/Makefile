# Makefile for codelibrary DTD
# $Id: Makefile,v 1.6 2003/07/16 13:14:33 fullermd Exp $
#
# Pretty much just install this stuff and go home.

INSTDIR ?= /usr/local/share/sgml
VERSION = 2.0
DIRPERMS ?= 755
FILEPERMS ?= 444
INSTUMASK ?= 022
VERSIONDIR = ${INSTDIR}/codelibrary/${VERSION}

SUBDIRS = css dsssl examples dtd

install:
	@echo Installing files...
	@umask ${INSTUMASK} && mkdir -p -m ${DIRPERMS} ${VERSIONDIR}
# Oh, this is SUCH a cheapass way of doing this...
	@install -c -m ${FILEPERMS} `find . -type f -maxdepth 1 -print | \
			grep -v Makefile` ${VERSIONDIR}/
	@for dir in ${SUBDIRS}; do \
		mkdir -p -m ${DIRPERMS} ${VERSIONDIR}/$${dir}; \
		( cd $${dir} && \
			install -c -m ${FILEPERMS} `find . -type f -maxdepth 1 -print` \
				${VERSIONDIR}/$${dir}/ ); \
		done

# Bit of a hack to fixup the sample Makefile
	@echo Fixing up sample Makefile...
	@sed "s%^DTDDIR.*%DTDDIR\ =\ ${VERSIONDIR}%" \
		${VERSIONDIR}/examples/Makefile.sample > \
		${VERSIONDIR}/examples/Makefile.sample.tmp
	@umask ${INSTUMASK} && mv -f ${VERSIONDIR}/examples/Makefile.sample.tmp \
		${VERSIONDIR}/examples/Makefile.sample

# Build a global catalog file for codelibrary DTD's
	@echo Building global catalog file...
	@( cd ${INSTDIR}/codelibrary && \
		umask ${INSTUMASK} && cp -f /dev/null catalog && \
		for dir in `echo */catalog`; do \
			umask ${INSTUMASK} && echo CATALOG \"$${dir}\" >> catalog; \
		done )

# Fin
	@echo Install successful!
