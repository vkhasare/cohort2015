SERVER=server
CLIENT=client

SERVER_STR=server_stripped
CLIENT_STR=client_stripped

SERVER_SOURCE=server.c server_ll.c server_fsm.c rbt.c misc.c 
CLIENT_SOURCE=client.c client_ll.c moderator_fsm.c

L_INCLUDE=-I${GFLAGS_BR}/include \
						 -I${GLOG_BR}/include
L_LIB_DIRS=-L${GFLAGS_BR}/lib \
				 -L${GLOG_BR}/lib
MY_LIBS=-lglog -lgflags
#MY_LIBS=-lglog -lgflags_nothreads

LC_STR=${L_INCLUDE} ${L_LIB_DIRS} ${MY_LIBS} -Wl,-rpath=${GFLAGS_BR}/lib:${GLOG_BR}/lib

CC=gcc
CPP=g++
CFLAGS=-g -c -Werror
#CFLAGS=-g -W -O2

logger:
	$(CPP) -fPIC -shared -o logg.so ${SRC}/logg.cpp	-I${INCLUDE} ${LC_STR}
	  
DEPS=sn_ll.c common.c receiver.c comm_primitives.c
DEPS_O=$(subst .c,.o,$(DEPS))

S_DEPS_O=$(subst .c,.o,$(SERVER_SOURCE))
C_DEPS_O=$(subst .c,.o,$(CLIENT_SOURCE))

deps:
	$(foreach var,$(DEPS),${CC} ${CFLAGS} ${SRC}/$(var) -I${INCLUDE};)
	$(foreach var,$(SERVER_SOURCE),${CC} ${CFLAGS} ${SRC}/$(var) -I${INCLUDE};)
	$(foreach var,$(CLIENT_SOURCE),${CC} ${CFLAGS} ${SRC}/$(var) -I${INCLUDE};)

$(SERVER): logger deps
	$(CPP) -o../$(SERVER) ${OBJ}/logg.so ${COMM_OBJ} ${DEPS_O} ${S_DEPS_O} \
	${LC_STR} -lrt

$(CLIENT): logger deps
	$(CPP) -o../$(CLIENT) ${OBJ}/logg.so ${COMM_OBJ} ${DEPS_O} ${C_DEPS_O} \
	${LC_STR} -lpthread -lrt

$(SERVER_STR): deps
	$(CPP) -o../$(SERVER) ${COMM_OBJ} ${DEPS_O} ${S_DEPS_O} -lrt

$(CLIENT_STR): deps
	$(CPP) -o../$(CLIENT) ${COMM_OBJ} ${DEPS_O} ${C_DEPS_O} -lpthread -lrt

.PHONY: dummy_rule




######################  SCRATCH-PAD  #########################

AR=ar
AR_OPT=cvq
COMM_INC = ${INCLUDE}/comm_primitives.h
COMM_SRC = ${SRC}/comm_primitives.c 
COMM_OUT = commprimitives.a
COMM_OBJ = $(($(patsubst ${SRC}/,'',COMM_SRC)):.c=.o)

comm : $(COMM_OUT)

$(COMM_OBJ) : $(COMM_SRC) $(COMM_INC)
	$(CC) $(CFLAGS) $(COMM_SRC) -I${INCLUDE}

$(COMM_OUT) : $(COMM_OBJ)
	$(AR) $(AR_OPT) $(COMM_OUT) $(COMM_OBJ)
#rm -f $(COMM_OBJ)

#dummy_rule:
#	echo -e 'trial ${PWD}\n\n${GLOG_BR}\n\n${GFLAGS_BR}'

#logger:
#	$(CPP) -fPIC -shared -o logg.so ${SRC}/logg.cpp	-I${INCLUDE} ${LC_STR}
	  
#$(CPP) -c ${SRC}/logg.cpp	-I${INCLUDE} ${L_INCLUDE} 

#dummy_rule:
#	echo -e 'trial ${PWD}\n\n${GLOG_BR}\n\n${GFLAGS_BR}'
