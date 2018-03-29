@echo off
cd /d %~dp0
SET include=-I ../include
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.vert.spv OITAppModel.vert
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.frag.spv OITAppModel.frag

glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.comp.spv PhotonMapping.comp
glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.vert.spv PhotonMapping.vert
glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.frag.spv PhotonMapping.frag

glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentHierarchy.comp.spv MakeFragmentHierarchy.comp
glslc.exe -w %include% -x glsl -o ../binary\\LightCulling.comp.spv LightCulling.comp
glslc.exe -w %include% -x glsl -o ../binary\\PMRendering.vert.spv PMRendering.vert
glslc.exe -w %include% -x glsl -o ../binary\\PMRendering.frag.spv PMRendering.frag


rem glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.geom.spv OITAppModel.geom


