@echo off
cd /d %~dp0

glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=geometry	-o binary\\Render.geom.spv Render.geom
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag

glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\OcclusionTest.vert.spv OcclusionTest.vert
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\OcclusionTest.frag.spv OcclusionTest.frag
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\WriteDepth.vert.spv WriteDepth.vert
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\WriteDepth.frag.spv WriteDepth.frag
glslc.exe -w -I Include -I ../../btrlib/shader/voxelize/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\CalcVisibleGeometry.comp.spv CalcVisibleGeometry.comp
