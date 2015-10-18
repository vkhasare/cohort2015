SHELL := /bin/bash
ROOT=${PWD}
TAR_ROOT=${ROOT}/utility
UTIL_BUILD_ROOT=${TAR_ROOT}/builddirs
CMAKE_BR=${UTIL_BUILD_ROOT}/cmake
GLOG_BR=${UTIL_BUILD_ROOT}/glog
GFLAGS_BR=${UTIL_BUILD_ROOT}/gflags

SRC=${ROOT}/src
INCLUDE=${ROOT}/include
OBJ=${ROOT}/obj

# cd ${UTIL_BUILD_ROOT}
#	tar -xvf ${TAR_ROOT}/gflags-2.1.2.tar -C GFLAGS_BR; 
#	tar -xvf ${TAR_ROOT}/glog-master.tar -C GLOG_BR; 

apps: comm server client 
all: utils apps
utils: build_gflags build_glog

create_build_env:
	mkdir -p ${UTIL_BUILD_ROOT}; \
  mkdir -p ${CMAKE_BR}; \
  mkdir -p ${GFLAGS_BR}; \
  mkdir -p ${GLOG_BR}; \
	mkdir -p ${OBJ}; \
  gcc ${SRC}/checker.c -o ${OBJ}/checker

build_cmake: create_build_env 
	(${OBJ}/checker cmake && ${MAKE} build_gflags_real) || ( \
	${OBJ}/checker /router/bin/cmake && \
  ([ $$? -eq 0 ] && export PATH=${PATH}:/router/bin && ${MAKE} build_gflags_real ) || ( \
	cd ${CMAKE_BR} && ${TAR_ROOT}/cmake-2.8.11/configure --prefix=${CMAKE_BR}; \
  make && make install; \
  export PATH=${PATH}:${CMAKE_BR}/bin; \
	cd ${ROOT}; ${MAKE} build_gflags_real; ))

build_gflags_real: create_build_env 
	cd ${GFLAGS_BR}; \
	cmake ${TAR_ROOT}/gflags-2.1.2 -DBUILD_SHARED_LIBS=ON -DBUILD_STATIC_LIBS=ON \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_INSTALL_PREFIX=${GFLAGS_BR} -DLIBRARY_INSTALL_DIR=${GFLAGS_BR}/lib; \
	make && make install;

build_gflags:
	${MAKE} build_cmake;

build_glog: create_build_env build_gflags
	export CPPFLAGS=-I${GFLAGS_BR}/include; \
	export LDFLAGS=-L${GFLAGS_BR}/lib; \
	LIBS=-lgflags; \
	cd ${GLOG_BR}; \
	${TAR_ROOT}/glog-master/configure --prefix=${GLOG_BR} --exec-prefix=${GLOG_BR} --datadir=${GLOG_BR} --docdir=${GLOG_BR} --oldincludedir=${GLOG_BR}; \
	make && make install; \
	unset LIBS CPPFLAGS LDFLAGS;

#all: build_cmake build_gflags build_glog comm server client
server:
	export GLOG_BR=${GLOG_BR} GFLAGS_BR=${GFLAGS_BR} INCLUDE=${INCLUDE} \
	SRC=${SRC} OBJ=${OBJ}; \
	cd obj/; \
	${MAKE} -f ../src/makefile server

client:
	export GLOG_BR=${GLOG_BR} GFLAGS_BR=${GFLAGS_BR} INCLUDE=${INCLUDE} \
	SRC=${SRC} OBJ=${OBJ}; \
	cd obj/; \
	${MAKE} -f ../src/makefile client

comm:
	export GLOG_BR=${GLOG_BR} GFLAGS_BR=${GFLAGS_BR} INCLUDE=${INCLUDE} \
	SRC=${SRC} OBJ=${OBJ}; \
	cd obj/; \
	${MAKE} -f ../src/makefile comm

.PHONY: build_cmake build_gflags_real

clean:
	rm -rf ${OBJ}/* ${ROOT}/server ${ROOT}/client;

clean_all:
	rm -rf ${OBJ}/* ${ROOT}/server ${ROOT}/client ${UTIL_BUILD_ROOT};
