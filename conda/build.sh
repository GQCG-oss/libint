#!/usr/bin/env bash

./autogen.sh
mkdir build && cd build
../configure --prefix=${PREFIX} --enable-shared=yes --enable-static=no CXXFLAGS="-O2 -std=c++11 -I${PREFIX}/include/ -L${PREFIX}/lib -lboost"
make -j${CPU_COUNT} VERBOSE=1 && make check && make install
