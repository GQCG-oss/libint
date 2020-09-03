#!/usr/bin/env bash

./autogen.sh
mkdir build && cd build
if [ `uname` == Darwin ]; then
    ../configure \ 
        --prefix=${PREFIX} \
        --enable-shared=yes \ 
        --enable-static=no \
        --with-incdirs=-I${PREFIX}/include \
        CC=${CLANG} \
        CXX=${CLANGXX} \
        CFLAGS="${CFLAGS} ${OPTS}" \ 
        CXXFLAGS="${CXXFLAGS} ${OPTS}"
fi
if [ `uname` == Linux ]; then
    ../configure \ 
        --prefix=${PREFIX} \
        --enable-shared=yes \ 
        --enable-static=no \
        --with-incdirs=-I${PREFIX}/include \
        CC=${GCC} \
        CXX=${GXX} \
        CFLAGS="${CFLAGS} ${OPTS}" \ 
        CXXFLAGS="${CXXFLAGS} ${OPTS}"
fi

echo "starting compilation"

make -j ${CPU_COUNT}
make install
