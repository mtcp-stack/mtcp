LIBS = -lps
LIB_DIR = -L../../lib

.PHONY: clean

all: rxdump

rxdump: rxdump.c
	gcc -O2 -g -Wall -o rxdump rxdump.c $(LIB_DIR) $(LIBS)

clean:
	rm -f rxdump
