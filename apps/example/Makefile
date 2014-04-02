CC = gcc
#CFLAGS = -Wall -g -DINFO -DDBGERR
CFLAGS = -DNDEBUG -O3 -DINFO -DDBGERR

TARGET = epserver epwget

UTIL_FLD = ../../util
UTIL_INC = ${UTIL_FLD}/include

EPSERVER_OBJS = epserver.o ${UTIL_FLD}/http_parsing.o ${UTIL_FLD}/tdate_parse.o
EPWGET_OBJS = epwget.o ${UTIL_FLD}/rss.o ${UTIL_FLD}/http_parsing.o ${UTIL_FLD}/tdate_parse.o

MTCP_FLD = ../../mtcp/lib
MTCP_INC = ../../mtcp/include
MTCP_LIB = ${MTCP_FLD}/libmtcp.a

PS_FLD = ../../io_engine/lib
PS_INC = ../../io_engine/include

INC = -I./include/ -I${UTIL_INC} -I${MTCP_INC} -I${PS_INC}
LIBS = -lnuma -lmtcp -lps -lpthread -lrt
LIB = -L${PS_FLD} -L${MTCP_FLD}

all: epserver epwget

%.o: %.c
	${CC} -c ${CFLAGS} ${INC} -o $@ $<

epserver: ${EPSERVER_OBJS} ${MTCP_LIB}
	${CC} -o epserver ${EPSERVER_OBJS} ${LIB} ${LIBS}

epwget: ${EPWGET_OBJS}  ${MTCP_LIB}
	${CC} -o epwget ${EPWGET_OBJS} ${LIB} ${LIBS}

clean:
	rm -f *~ *.o ${TARGET}
