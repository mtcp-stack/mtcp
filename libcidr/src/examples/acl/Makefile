# Temporary Makefile just to handle early testing
C_FILES = acl.c
C_FLAGS = -I../../../include
CLEAN_FILES = ${O_FILES} acl *core .depend

L_FLAGS = -g -pipe -L../.. -lcidr

.include "../../Makefile.inc"

# This need to be after the include
all build: acl

LCIDR = ../../libcidr.so

acl: ${O_FILES} ${LCIDR}
	${CC} ${L_FLAGS} -o ${@} ${O_FILES}

${LCIDR}:
	(cd ../.. && make)
