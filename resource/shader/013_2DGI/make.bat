@echo off
cd /d %~dp0
SET include=-I ../include
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.vert.spv OITAppModel.vert
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.frag.spv OITAppModel.frag
rem glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.geom.spv OITAppModel.geom


