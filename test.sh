#!/usr/bin/env bash

set -o errexit

if [ ! -d out ]; then
    CXX=clang++ \
    meson out
fi
meson install -C out
./packaged/bin/bar
