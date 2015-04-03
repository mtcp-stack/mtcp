LIBS = -lps -lnuma
LIB_DIR = -L../../lib

.PHONY: clean

all: packet_generator

packet_generator: packet_generator.c
	gcc -O2 -g -Wall -o packet_generator packet_generator.c $(LIB_DIR) $(LIBS)

clean:
	rm -f packet_generator
