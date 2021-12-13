@echo off
cmake .. -B "..\build_vs2022_x64" -G"Visual Studio 17" -A x64 -DMATHPRESSO_TEST=1
