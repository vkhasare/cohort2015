#SOURCE=server.c common.h logg.h
SERVER_SOURCE=sn_ll.c server.c common.c receiver.c
CLIENT_SOURCE=sn_ll.c client.c common.c receiver.c
SERVER=server
CLIENT=client
MYINCLUDES=/usr/local/lib
CMAKE_PATH=${PWD}/cmake-2.8.11
GFLAGDIRS=/root/gflags-2.1.2
CMAKEDIRS=/root/cmake-2.8.11
GLOGDIRS=/root/glog-master
BUILD=buildgflags
MYLIBRARIES=glog
CPP=gcc
CPPFLAGS=-g
AR=ar
AR_OPT=cvq
#CPPFLAGS=-g -W -O2


build_cmake: 
	cp -r ${PWD}/utility/cmake-2.8.11.tar /root/; cd /root; tar -xvf cmake-2.8.11.tar; cd $(CMAKEDIRS) \
          ./configure; \
    make && make install; \
    rm /usr/bin/cmake; \
    ln -s /usr/local/bin/cmake /usr/bin/cmake; \


build_gflags: 
	cp -r ${PWD}/utility/gflags-2.1.2.tar /root/; cd /root; tar -xvf gflags-2.1.2.tar; cd $(GFLAGDIRS) \
	mkdir $(BUILD); \
    for subdir in $(BUILD); do\
    cd $$subdir; \
    ccmake ..; \
    make && make install; \
    sudo ldconfig; \
    done

build_glog:
	cp -r ${PWD}/utility/glog-master.tar /root/; cd /root; tar -xvf glog-master.tar; cd $(GLOGDIRS) \
          ./configure; \
        make && make install; \

server: $(SERVER)
client: $(CLIENT)

#all: build_cmake build_gflags build_glog comm server client

all: comm server client

$(SERVER): $(SERVER_SOURCE) $(COMM_OUT)
	$(CPP) $(CPPFLAGS) $(SERVER_SOURCE) -o$(SERVER) $(COMM_OUT)
#	$(CPP) $(CPPFLAGS) -I$(MYINCLUDES) $(SERVER_SOURCE) -o$(SERVER)  $(COMM_OUT) -l$(MYLIBRARIES)

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
	rm -f $(COMM_OBJ)

clean:
	rm -f $(SERVER)
	rm -f $(CLIENT)
	rm -f $(COMM_OBJ) $(COMM_OUT)
	rm -f *.h.gch
