#!/bin/sh

CURRENT_DIR=`pwd`
BUILD_DIR="build_xcode"

mkdir -p ../${BUILD_DIR}
cd ../${BUILD_DIR}
cmake .. -G"Xcode" -DMATHPRESSO_TEST=1
cd ${CURRENT_DIR}
