#!/bin/sh
cmake -DCMAKE_BUILD_TYPE=Debug ..
make clean
make
if [ ! -x bin ]; then
    mkdir bin
fi
if [ ! -x lib ]; then
    mkdir lib
fi
cp -vf app/trigg_miner/trigg bin
cp -vf app/testlib/testlib bin
cp -vf libclib.a lib

