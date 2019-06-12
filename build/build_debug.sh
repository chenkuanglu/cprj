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
cp -f app/trigg_miner/trigg bin
cp -f app/test/test bin
cp -f libclib.a lib

