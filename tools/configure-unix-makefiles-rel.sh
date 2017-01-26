#!/bin/sh

CURRENT_DIR=`pwd`
BUILD_DIR="build_makefiles_rel"
ASMJIT_DIR="../../asmjit"

mkdir -p ../${BUILD_DIR}
cd ../${BUILD_DIR}
cmake .. -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DASMJIT_DIR="${ASMJIT_DIR}" -DMATHPRESSO_BUILD_TEST=1
cd ${CURRENT_DIR}
