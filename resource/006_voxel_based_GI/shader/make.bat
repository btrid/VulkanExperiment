@echo off
cd /d %~dp0

glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\ModelVoxelize.vert.spv ModelVoxelize.vert
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=geometry	-o binary\\ModelVoxelize.geom.spv ModelVoxelize.geom
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\ModelVoxelize.frag.spv ModelVoxelize.frag

glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\DrawAlbedoVoxel.vert.spv DrawAlbedoVoxel.vert
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\DrawAlbedoVoxel.frag.spv DrawAlbedoVoxel.frag

