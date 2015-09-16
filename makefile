SOURCE=server.c common.h logg.h
MYPROGRAM=server
MYINCLUDES=/usr/local/lib
CMAKE_PATH=${PWD}/cmake-2.8.11
GFLAGDIRS=gflags-2.1.2
CMAKEDIRS=cmake-2.8.11
GLOGDIRS=glog-master
BUILD=buildgflags
MYLIBRARIES=glog
CPP=g++
CPPFLAGS=-g -W -O2

build_cmake: 
		for dir in $(CMAKEDIRS); do \
			cd $$dir; \
          ./configure; \
		make && make install; \
		rm /usr/bin/cmake; \
		ln -s /usr/local/bin/cmake /usr/bin/cmake; \
        done


build_gflags: 
		for dir in $(GFLAGDIRS); do \
		cd $$dir; \
		mkdir $(BUILD); \
		for subdir in $(BUILD); do\
		cd $$subdir; \
		ccmake ..; \
		make && make test && make install; \
		sudo ldconfig; \
		done; \
        done

build_glog:
		for dir in $(GLOGDIRS); do \
            cd $$dir; \
          ./configure; \
        make && make install; \
        done

server: $(MYPROGRAM)

all: build_cmake build_gflags build_glog server

$(MYPROGRAM): $(SOURCE)

	$(CPP) $(CPPFLAGS) -I$(MYINCLUDES) $(SOURCE) -o$(MYPROGRAM) -l$(MYLIBRARIES) $<

clean:

	rm -f $(MYPROGRAM)

