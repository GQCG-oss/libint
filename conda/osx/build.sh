#!/usr/bin/env bash

# configure
./configure --prefix=${PREFIX} --enable-shared=yes --enable-static=no CXXFLAGS="-O2 -std=c++11"

# build
make -j${CPU_COUNT}
make check

# install
make install

# check
#LD_LIBRARY_PATH=${PREFIX}/lib make check
