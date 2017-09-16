@echo off
cd /d %~dp0

glslangValidator.exe -w -IInclude -I../../btrlib/shader/include -V -o binary\\Voxelize.vert.spv Voxelize.vert
glslangValidator.exe -w -IInclude -I../../btrlib/shader/include -V -o binary\\Voxelize.geom.spv Voxelize.geom
glslangValidator.exe -w -IInclude -I../../btrlib/shader/include -V -o binary\\Voxelize.frag.spv Voxelize.frag
