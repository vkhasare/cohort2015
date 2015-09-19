#SOURCE=server.c common.h logg.h
SERVER_SOURCE=server.c common.c receiver.c
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
#CPPFLAGS=-g -W -O2

#build_cmake: 
#		for dir in $(CMAKEDIRS); do \
			cd $$dir; \
          ./configure; \
		make && make install; \
		rm /usr/bin/cmake; \
		ln -s /usr/local/bin/cmake /usr/bin/cmake; \
        done


#build_gflags: 
#		for dir in $(GFLAGDIRS); do \
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
#		for dir in $(GLOGDIRS); do \
            cd $$dir; \
          ./configure; \
        make && make install; \
        done

server: $(SERVER)
client: $(CLIENT)

#all: build_cmake build_gflags build_glog server

all: server client

$(SERVER): $(SERVER_SOURCE)

	$(CPP) $(CPPFLAGS) $(SERVER_SOURCE) -o$(SERVER)
#	$(CPP) $(CPPFLAGS) -I$(MYINCLUDES) $(SERVER_SOURCE) -o$(SERVER) -l$(MYLIBRARIES) $<

$(CLIENT): $(CLIENT_SOURCE)

	$(CPP) $(CPPFLAGS) $(CLIENT_SOURCE) -o$(CLIENT)

clean:

	rm -f $(SERVER)
	rm -f $(CLIENT)

