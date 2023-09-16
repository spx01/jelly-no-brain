#!/bin/bash

set -ve
cd "$(dirname "${BASH_SOURCE[0]}")"

if [[ "$1" == "release" ]]; then
    echo "Building in release mode"
    EXTRA_FLAGS="-O3"
else
    EXTRA_FLAGS="-Og -g"
fi

python3 gen_funclist.py

FILES="src/game.c src/web.c src/util.c src/b64.c"

mkdir -p web
eval emcc -o web/jnb.html $FILES \
    $EXTRA_FLAGS \
    -sWASM=1 \
    -Wall \
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap \
    $(cat emcc_funclist.txt)
