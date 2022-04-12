#!/usr/bin/env bash 
set -e 
TOP="$(dirname "$0")"
pushd "$TOP/../build"
make -j$(nproc)
popd

../build/libmisc_test