#!/bin/sh

CURRENT_DIR=`pwd`
BUILD_DIR="build"
BUILD_OPTIONS="-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMATHPRESSO_TEST=1"

mkdir -p ../${BUILD_DIR}_dbg
mkdir -p ../${BUILD_DIR}_rel

cd ../${BUILD_DIR}_dbg
eval cmake .. -G"Ninja" -DCMAKE_BUILD_TYPE=Debug ${BUILD_OPTIONS}
cd ${CURRENT_DIR}

cd ../${BUILD_DIR}_rel
eval cmake .. -G"Ninja" -DCMAKE_BUILD_TYPE=Release ${BUILD_OPTIONS}
cd ${CURRENT_DIR}
