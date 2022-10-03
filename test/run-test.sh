#!/usr/bin/env bash 
set -e 

cd "$(dirname "$0")"

if [ ! -f "./large_test.kv" ]; then
	./gen-test.py
fi

pushd "../build"
make -j$(nproc)
popd > /dev/null

../build/libmisc_test
