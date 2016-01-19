#!/bin/sh
# Quick hack of a tool to make a pristine tarball of libcidr
# Minimal seatbelts

dstdir=$1

if [ "X${dstdir}" = "X" ]; then
	echo "You better tell me where to put it, buddy!"
	exit
fi

if [ -r ${dstdir} ]; then
	echo "Sorry, I'm not running on a directory that exists."
	exit
fi


bzr checkout --lightweight .. ${dstdir}
(cd ${dstdir}/src && sed -i "" -e "s/-Werror/#-Werror/" Makefile.inc )
(cd ${dstdir} && sh mkgmake.sh)
(cd ${dstdir}/docs/reference/sgml/ && make all clean)
(cd ${dstdir}/include && \
	revid=`bzr testament | grep ^revision-id | awk '{print $2}'` ; \
	sed -i "" -e "s/CIDR_REVISION \"\"/CIDR_REVISION \" (${revid})\"/" \
			libcidr.h )
(cd ${dstdir} && rm -rf .bzr .bzrignore)
