@echo off
cd /d %~dp0
SET include=-I ../include
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.vert.spv OITAppModel.vert
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.frag.spv OITAppModel.frag

glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.comp.spv PhotonMapping.comp
rem glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.geom.spv OITAppModel.geom


