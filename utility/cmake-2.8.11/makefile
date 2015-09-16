#!/bin/sh
SOURCE=server.c common.h logg.h
MYPROGRAM=server
MYINCLUDES=/usr/local/lib
CMAKE_PATH=$PWD/cmake-2.8.11
MYLIBRARIES=glog
CPP=g++

#cpp: apt-get install $(CPP)

#cmake: cd $PWD/cmake-2.8.11,./configure && make && make install,rm /usr/bin/cmake,ln -s /usr/local/bin/cmake /usr/bin/cmake,cd $PWD/
cmake_source_dir=`cd "\`dirname \"$0\"\`";pwd`
#exec "cd $PWD/cmake-2.8.11" "${cmake_source_dir}/bootstrap" "$@"

cmake: exec "${cmake_source_dir}/bootstrap" "$@"

#mkdir buildgflags \
cd buildgflags \
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DGFLAGS_NAMESPACE=google -G"Unix Makefiles" /usr/local/src/gflags-2.1.2/ \
make && make install \
sudo ldconfig \
cd $PWD/project_Albus_Dumbledore

gflags: cd $PWD/gflags-2.1.2 
#mkdir build && cd build;ccmake ..;c;c;g;make && make test && make install;cd $PWD

all: $(gflags) 

server: $(MYPROGRAM)

$(MYPROGRAM): $(SOURCE)

	$(CPP) -g -W -O2 -I$(MYINCLUDES) $(SOURCE) -o$(MYPROGRAM) -l$(MYLIBRARIES) $<

clean:

	rm -f $(MYPROGRAM)

