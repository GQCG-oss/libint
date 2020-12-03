#!/usr/bin/env bash

set -x
./autogen.sh
mkdir build && cd build
if [ `uname` == Darwin ]; then
    ../configure --prefix=${PREFIX} --enable-shared=yes --enable-static=no --with-incdirs=-I${PREFIX}/include --host=x86_64-apple-darwin19.6.0 --build=x86_64-apple-darwin19.6.0 --target=x86_64-apple-darwin19.6.0 CC=${CLANG} CXX=${CLANGXX} CFLAGS="${CFLAGS} ${OPTS} -isysroot ${CONDA_BUILD_SYSROOT}" CXXFLAGS="${CXXFLAGS} ${OPTS} -isysroot ${CONDA_BUILD_SYSROOT}"
fi
if [ `uname` == Linux ]; then
    ../configure --prefix=${PREFIX} --enable-shared=yes --enable-static=no --with-incdirs=-I${PREFIX}/include --host=x86_64-pc-linux-gnu --build=x86_64-pc-linux-gnu --target=x86_64-pc-linux-gnu CC=${GCC} CXX=${GXX} CFLAGS="${CFLAGS} ${OPTS} -fopenmp" CXXFLAGS="${CXXFLAGS} ${OPTS} -fopenmp"
fi

make -j ${CPU_COUNT}
make install
make check
set +x
