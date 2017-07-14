CC=gcc
PROGRAM=quickhpc
PAPI_DIR=./papi-5.5.1/src
LIBS_STATIC=${PAPI_DIR}/libpapi.a
OBJS=config.o
EXTERNAL_OBJS=${PAPI_DIR}/testlib/do_loops.o \
	 ${PAPI_DIR}/testlib/test_utils.o \
	 ${PAPI_DIR}/testlib/dummy.o
INC=-I${PAPI_DIR} -I${PAPI_DIR} -I${PAPI_DIR}/testlib


all: quickhpc

quickhpc: config 
	gcc ${INC} -g -Wall quickhpc.c \
		${EXTERNAL_OBJS} \
		${LIBS_STATIC} \
		config.o -o quickhpc

debug: CC += -D_DEBUG
debug: all

config:
	${CC} ${INC} -g -c config.c

clean:
	-rm -f ${PROGRAM}
	-rm -f ${OBJS}
