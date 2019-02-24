CC=gcc
PAPI_DIR=/path/to/papi-5.4.1/src
EXT_OBJS=${PAPI_DIR}/testlib/do_loops.o \
	 ${PAPI_DIR}/testlib/test_utils.o \
	 ${PAPI_DIR}/testlib/dummy.o

all:  
	${CC} -I${PAPI_DIR} -I${PAPI_DIR} -I${PAPI_DIR}/testlib \ 
		-g -Wall papitool.c argparse.c \
		${EXT_OBJS} \
		${PAPI_DIR}/libpapi.a \
		-o papitool

clean:
	-rm papitool
