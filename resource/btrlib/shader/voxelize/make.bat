@echo off
cd /d %~dp0

glslc.exe -w -I ../include -x glsl -fshader-stage=vertex	-o ../binary\\DrawAlbedoVoxel.vert.spv DrawAlbedoVoxel.vert
glslc.exe -w -I ../include -x glsl -fshader-stage=fragment	-o ../binary\\DrawAlbedoVoxel.frag.spv DrawAlbedoVoxel.frag

glslc.exe -w -I ../include -x glsl -fshader-stage=compute	-o ../binary\\MakeVoxelHierarchy.comp.spv MakeVoxelHierarchy.comp

