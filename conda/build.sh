#!/usr/bin/env bash

./autogen.sh
mkdir build && cd build
if [ `uname` == Darwin ]; then
    ../configure \ 
        --prefix=${PREFIX} \
        --enable-shared=yes \ 
        --enable-static=no \
        CC=${CLANG} \
        CXX=${CLANGXX} \
        CFLAGS="${CFLAGS} ${OPTS} -I${PREFIX}/include" \ 
        CXXFLAGS="${CXXFLAGS} ${OPTS} -I${PREFIX}/include"
fi
if [ `uname` == Linux ]; then
    ../configure \ 
        --prefix=${PREFIX} \
        --enable-shared=yes \ 
        --enable-static=no \
        CC=${GCC} \
        CXX=${GXX} \
        CFLAGS="${CFLAGS} ${OPTS} -I${PREFIX}/include" \ 
        CXXFLAGS="${CXXFLAGS} ${OPTS} -I${PREFIX}/include"
fi

make -j${CPU_COUNT} VERBOSE=1 && make check && make install
