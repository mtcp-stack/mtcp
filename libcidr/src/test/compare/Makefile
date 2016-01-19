# Temporary Makefile just to handle early testing
C_FILES = compare.c
C_FLAGS = -I../../../include
CLEAN_FILES = ${O_FILES} cidr_compare *core .depend

L_FLAGS = -g -pipe -L../.. -lcidr

.include "../../Makefile.inc"

# This need to be after the include
all build: cidr_compare

LCIDR = ../../libcidr.so

cidr_compare: ${O_FILES} ${LCIDR}
	${CC} ${L_FLAGS} -o ${@} ${O_FILES}

${LCIDR}:
	(cd ../.. && make)
