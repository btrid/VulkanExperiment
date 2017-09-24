@echo off
cd /d %~dp0

glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\ModelVoxelize.vert.spv ModelVoxelize.vert
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=geometry	-o binary\\ModelVoxelize.geom.spv ModelVoxelize.geom
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\ModelVoxelize.frag.spv ModelVoxelize.frag

