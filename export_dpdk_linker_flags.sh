#!/bin/bash
sed -i -e 's/O_TO_EXE_STR =/\$(shell if [ \! -d \${RTE_SDK}\/\${RTE_TARGET}\/lib ]\; then mkdir \${RTE_SDK}\/\${RTE_TARGET}\/lib\; fi)\nLINKER_FLAGS = \$(call linkerprefix,\$(LDLIBS))\n\$(shell echo \${LINKER_FLAGS} \> \${RTE_SDK}\/\${RTE_TARGET}\/lib\/ldflags\.txt)\nO_TO_EXE_STR =/g' $RTE_SDK/mk/rte.app.mk
