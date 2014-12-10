#!/usr/bin/env bash

if [ $BUILD_TYPE == "default" ]; then
    # Clone and build dependencies
    git clone https://github.com/zeromq/libzmq zmq
    ( cd zmq && ./autogen.sh && ./configure && make check && sudo make install && sudo ldconfig )

    git clone https://github.com/zeromq/czmq czmq
    ( cd czmq && ./autogen.sh && ./configure && make check && sudo make install && sudo ldconfig )

    # Build and check this project
    ./autogen.sh && ./configure && make && make check && make memcheck && sudo make install
else
    cd ./builds/${BUILD_TYPE} && ./ci_build.sh
fi
