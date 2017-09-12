@echo off
cd /d %~dp0

glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\Voxelize.vert.spv Voxelize.vert
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=geometry	-o binary\\Voxelize.geom.spv Voxelize.geom
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\Voxelize.frag.spv Voxelize.frag
