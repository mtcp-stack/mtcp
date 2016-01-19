# Temporary Makefile just to handle early testing
C_FILES = inaddr.c
C_FLAGS = -I../../../include
CLEAN_FILES = ${O_FILES} cidr_inaddr *core .depend

L_FLAGS = -g -pipe -L../.. -lcidr

.include "../../Makefile.inc"

# This need to be after the include
all build: cidr_inaddr

LCIDR = ../../libcidr.so

cidr_inaddr: ${O_FILES} ${LCIDR}
	${CC} ${L_FLAGS} -o ${@} ${O_FILES}

${LCIDR}:
	(cd ../.. && make)
