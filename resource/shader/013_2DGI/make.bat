@echo off
cd /d %~dp0
SET include=-I ../include
glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentAppModel.vert.spv MakeFragmentAppModel.vert
glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentAppModel.frag.spv MakeFragmentAppModel.frag

glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.comp.spv PhotonMapping.comp

glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentMap.comp.spv MakeFragmentMap.comp
glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentMapHierarchy.comp.spv MakeFragmentMapHierarchy.comp
glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentHierarchy.comp.spv MakeFragmentHierarchy.comp
glslc.exe -w %include% -x glsl -o ../binary\\LightCulling.comp.spv LightCulling.comp
glslc.exe -w %include% -x glsl -o ../binary\\PMRendering.vert.spv PMRendering.vert
glslc.exe -w %include% -x glsl -o ../binary\\PMRendering.frag.spv PMRendering.frag

glslc.exe -w %include% -x glsl -o ../binary/PMPointLight.comp.spv PMPointLight.comp
glslc.exe -w %include% -x glsl -o ../binary/PMMakeLightList.comp.spv PMMakeLightList.comp

glslc.exe -w %include% -x glsl -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp

