#!/bin/sh
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
if [ ! -x bin ]; then
    mkdir bin
fi
if [ ! -x lib ]; then
    mkdir lib
fi
cp app/trigg_miner/trigg bin
cp libclib.a lib

