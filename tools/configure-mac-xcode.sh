#!/bin/sh

CURRENT_DIR=`pwd`
BUILD_DIR="build_xcode"
ASMJIT_DIR="../../asmjit"

mkdir -p ../${BUILD_DIR}
cd ../${BUILD_DIR}
cmake .. -G"Xcode" -DASMJIT_DIR="${ASMJIT_DIR}" -DMATHPRESSO_BUILD_TEST=1
cd ${CURRENT_DIR}
