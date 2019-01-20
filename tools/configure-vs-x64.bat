@echo off

set CURRENT_DIR=%CD%
set BUILD_DIR="build_vs_x64"

mkdir ..\%BUILD_DIR%
cd ..\%BUILD_DIR%
cmake .. -G"Visual Studio 16" -A x64 -DMATHPRESSO_TEST=1
cd %CURRENT_DIR%
