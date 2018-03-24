@echo off
cd /d %~dp0
SET include=-I ../../include
SET output=-o ../../binary/

glslc.exe %include% -w -x glsl %output%PhotonMapping2.comp.spv PhotonMapping2.comp
glslc.exe %include% -w -x glsl %output%MakeFragmentHierarchy2.comp.spv MakeFragmentHierarchy2.comp
glslc.exe %include% -w -x glsl %output%LightCulling2.comp.spv LightCulling2.comp
glslc.exe %include% -w -x glsl %output%PMRendering2.vert.spv PMRendering2.vert
glslc.exe %include% -w -x glsl %output%PMRendering2.frag.spv PMRendering2.frag

rem glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.geom.spv OITAppModel.geom


