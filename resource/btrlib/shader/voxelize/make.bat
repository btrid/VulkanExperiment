@echo off
cd /d %~dp0

glslc.exe -w -I ../include -x glsl -fshader-stage=vertex	-o ../binary\\DrawVoxel.vert.spv DrawVoxel.vert
glslc.exe -w -I ../include -x glsl -fshader-stage=fragment	-o ../binary\\DrawVoxel.frag.spv DrawVoxel.frag

glslc.exe -w -I ../include -x glsl -fshader-stage=compute	-o ../binary\\MakeVoxelHierarchy.comp.spv MakeVoxelHierarchy.comp

