### GCC ###
GCC = gcc
GCC_OPT = -m64 -Wall -fPIC -c
GCC_OPT += -DNDEBUG -O3 -DNETSTAT -DINFO -DDBGERR -DDBGCERR


### LIBRARIES AND INCLUDES ###
CFLAGS = -I./include/


### COMPILE ###
all:
	${CC} ${GCC_OPT} ${CFLAGS} tdate_parse.c
	${CC} ${GCC_OPT} ${CFLAGS} http_parsing.c

clean:
	rm -rf *~ *.o

