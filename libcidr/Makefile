# Top-level Makefile for libcidr
# This basically calls out to various sub-level Makefiles for the build,
# and handles the bits&pieces of installing manually.

# Define some destination directories
PREFIX?=/usr/local
CIDR_LIBDIR?=${PREFIX}/lib
CIDR_BINDIR?=${PREFIX}/bin
CIDR_INCDIR?=${PREFIX}/include
CIDR_MANDIR?=${PREFIX}/man
CIDR_DOCDIR?=${PREFIX}/share/libcidr/docs
CIDR_EXDIR?=${PREFIX}/share/libcidr/examples

# A few programs
ECHO?=echo
MKDIR?=mkdir -pm 755
INSTALL?=install -c
LN?=ln
GZIP?=gzip
RM?=rm -f
SED?=sed

# Standard defines
.include "Makefile.inc"


# The building doesn't touch the docs, intentionally.  We presume they're
# pre-built if we care about them, because building them requires a lot
# of extra programs that many people won't have.
all build clean:
	(cd src && ${MAKE} ${@})
	(cd src/examples/cidrcalc && ${MAKE} ${@})


# Provide a quick&dirty 'uninstall' target
uninstall:
	@${ECHO} "-> Trying to delete everything libcidr-related..."
	${RM} ${CIDR_LIBDIR}/${SHLIB_NAME} \
	      ${CIDR_LIBDIR}/${SHLIB_LINK}
	${RM} ${CIDR_LIBDIR}/${STATICLIB_NAME} \
	      ${CIDR_LIBDIR}/${STATICLIB_LINK}
	${RM} ${CIDR_BINDIR}/cidrcalc
	${RM} ${CIDR_INCDIR}/libcidr.h
	${RM} ${CIDR_MANDIR}/man3/libcidr.3.gz
	${RM} -r ${CIDR_DOCDIR}
	${RM} -r ${CIDR_EXDIR}
	@${ECHO} "-> Uninstallation complete"

install-libcidr:
	@${ECHO} "-> Installing ${SHLIB_NAME}..."
	-@${MKDIR} ${CIDR_LIBDIR}
	${INSTALL} -m 444 src/${SHLIB_NAME} 	${CIDR_LIBDIR}/
	${INSTALL} -m 444 src/${STATICLIB_NAME} ${CIDR_LIBDIR}/
	( cd ${CIDR_LIBDIR}  && ${LN} -fs ${SHLIB_NAME} 	${SHLIB_LINK}  && ${LN} -fs ${STATICLIB_NAME}	${STATICLIB_LINK} )
	@${ECHO} "-> Installing manpage..."
	@${SED} -e 's|%%DOCDIR%%|${CIDR_DOCDIR}|' docs/libcidr.3 | \
			${GZIP} > docs/libcidr.3.gz
	-@${MKDIR} ${CIDR_MANDIR}/man3
	${INSTALL} -m 444 docs/libcidr.3.gz ${CIDR_MANDIR}/man3
	@${RM} docs/libcidr.3.gz
.ifndef NO_DOCS
	@${ECHO} "-> Installing docs..."
	-@${MKDIR} ${CIDR_DOCDIR}
	${INSTALL} -m 444 docs/reference/libcidr* \
			docs/reference/codelibrary-html.css ${CIDR_DOCDIR}/
.endif
.ifndef NO_EXAMPLES
	@${ECHO} "-> Installing examples..."
	-@${MKDIR} ${CIDR_EXDIR}
	@${SED} -e "s,\.\./include,${CIDR_INCDIR}," \
			-e "s,\.\./Makefile\.inc,/dev/null," \
			< src/Makefile.inc \
			> ${CIDR_EXDIR}/Makefile.inc
	${INSTALL} -m 444 src/examples/README ${CIDR_EXDIR}/
	@${MAKE} EX=cidrcalc install-example
	@${MAKE} EX=acl EXFILE=acl.example install-example
.endif

install-libcidr-dev:
	@${ECHO} "-> Installing header file..."
	-@${MKDIR} ${CIDR_INCDIR}
	${INSTALL} -m 444 include/libcidr.h ${CIDR_INCDIR}/

install-cidrcalc:
	@${ECHO} "-> Installing cidrcalc..."
	-@${MKDIR} ${CIDR_BINDIR}
	${INSTALL} -m 555 src/examples/cidrcalc/cidrcalc ${CIDR_BINDIR}/

# Now the bits of installing
install: install-libcidr install-libcidr-dev install-cidrcalc
	@${ECHO} ""
	@${ECHO} "libcidr install complete"


install-example:
	@${ECHO} "-> Installing examples/${EX}..."
	-@${MKDIR} ${CIDR_EXDIR}/${EX}
	${INSTALL} -m 444 src/examples/${EX}/${EX}.c ${CIDR_EXDIR}/${EX}/
.ifdef EXFILE
	${INSTALL} -m 444 src/examples/${EX}/${EXFILE} ${CIDR_EXDIR}/${EX}/
.endif
	@${SED} -e "s,\.\./\.\./\.\./include,${CIDR_INCDIR}," \
			-e "s,-L\.\./\.\.,-L${CIDR_LIBDIR}," \
			-e "s,\.\./\.\./libcidr.so,${CIDR_LIBDIR}/libcidr.so," \
			-e "s,cd\ \.\./\.\.\ &&\ make,cd," \
			-e "s,\.\./\.\./Makefile\.inc,\.\./Makefile.inc," \
			< src/examples/${EX}/Makefile \
			> ${CIDR_EXDIR}/${EX}/Makefile
