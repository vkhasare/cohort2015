#SOURCE=server.c common.h logg.h
SERVER_SOURCE=sn_ll.c server.c common.c receiver.c
CLIENT_SOURCE=sn_ll.c client.c common.c receiver.c
SERVER=server
CLIENT=client
MYINCLUDES=/usr/local/lib
CMAKE_PATH=${PWD}/cmake-2.8.11
GFLAGDIRS=gflags-2.1.2
CMAKEDIRS=cmake-2.8.11
GLOGDIRS=glog-master
BUILD=buildgflags
MYLIBRARIES=glog
CPP=gcc
CPPFLAGS=
AR=ar
AR_OPT=cvq
#CPPFLAGS=-g -W -O2


#build_cmake: 
#   for dir in $(CMAKEDIRS); do \
      cd $$dir; \
          ./configure; \
    make && make install; \
    rm /usr/bin/cmake; \
    ln -s /usr/local/bin/cmake /usr/bin/cmake; \
        done


#build_gflags: 
#   for dir in $(GFLAGDIRS); do \
    cd $$dir; \
    mkdir $(BUILD); \
    for subdir in $(BUILD); do\
    cd $$subdir; \
    ccmake ..; \
    make && make test && make install; \
    sudo ldconfig; \
    done; \
        done

#build_glog:
#   for dir in $(GLOGDIRS); do \
            cd $$dir; \
          ./configure; \
        make && make install; \
        done

server: $(SERVER)
client: $(CLIENT)

#all: build_cmake build_gflags build_glog server

all: comm server client

$(SERVER): $(SERVER_SOURCE) $(COMM_OUT)
	$(CPP) $(CPPFLAGS) $(SERVER_SOURCE) -o$(SERVER) $(COMM_OUT)
# $(CPP) $(CPPFLAGS) -I$(MYINCLUDES) $(SERVER_SOURCE) -o$(SERVER) -l$(MYLIBRARIES) $<

$(CLIENT): $(CLIENT_SOURCE) $(COMM_OUT)

	$(CPP) $(CPPFLAGS) $(CLIENT_SOURCE) -o$(CLIENT) $(COMM_OUT)

COMM_INC = comm_primitives.h
COMM_SRC = comm_primitives.c 
COMM_OUT = commprimitives.a
COMM_OBJ = $(COMM_SRC:.c=.o)
comm : $(COMM_OUT)

$(COMM_OBJ) : $(COMM_SOURCE)
	$(CPP) $(CPPFLAGS) -c $(COMM_SRC)

$(COMM_OUT) : $(COMM_OBJ)
	$(AR) $(AR_OPT) $(COMM_OUT) $(COMM_OBJ)

clean:
	rm -f $(SERVER)
	rm -f $(CLIENT)
	rm -f $(COMM_OBJ) $(COMM_OUT)
