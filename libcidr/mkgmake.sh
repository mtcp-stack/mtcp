#!/bin/sh

# Make GNU make Makefiles
MAKEFILES="Makefile Makefile.inc src/Makefile src/Makefile.inc
		src/examples/acl/Makefile src/examples/cidrcalc/Makefile
		src/test/Makefile
		src/test/compare/Makefile
		src/test/inaddr/Makefile
		src/test/kids/Makefile
		src/test/mkstr/Makefile
		src/test/netbc/Makefile
		src/test/nums/Makefile
		src/test/parent/Makefile
		docs/reference/sgml/Makefile"

case $1 in
	rm)
		for i in ${MAKEFILES}; do
			GMFILE=`echo ${i} | sed "s/Makefile/GNUmakefile/"`
			echo "rm -f ${GMFILE}"
			rm -f ${GMFILE}
		done
		;;
	*)
		for i in ${MAKEFILES}; do
			GMFILE=`echo ${i} | sed "s/Makefile/GNUmakefile/"`
			echo -n "Building ${GMFILE} from ${i}...   "
			awk -f tools/mkgmake.awk ${i} > ${GMFILE}
			echo done.
		done
		;;
esac
