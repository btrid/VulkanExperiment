@echo off
cd /d %~dp0

rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=geometry	-o binary\\Render.geom.spv Render.geom
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag

rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\OcclusionTest.vert.spv OcclusionTest.vert
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\OcclusionTest.frag.spv OcclusionTest.frag
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\WriteDepth.vert.spv WriteDepth.vert
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\WriteDepth.frag.spv WriteDepth.frag
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\CalcVisibleGeometry.comp.spv CalcVisibleGeometry.comp

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\TileCulling.comp.spv TileCulling.comp
