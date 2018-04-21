@echo off
cd /d %~dp0
SET include=-I ../include
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.vert.spv OITAppModel.vert
glslc.exe -w %include% -x glsl -o ../binary\\OITAppModel.frag.spv OITAppModel.frag

glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.comp.spv PhotonMapping.comp
rem glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.vert.spv PhotonMapping.vert
rem glslc.exe -w %include% -x glsl -o ../binary\\PhotonMapping.frag.spv PhotonMapping.frag

glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentMap.comp.spv MakeFragmentMap.comp
glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentMapHierarchy.comp.spv MakeFragmentMapHierarchy.comp
glslc.exe -w %include% -x glsl -o ../binary\\MakeFragmentHierarchy.comp.spv MakeFragmentHierarchy.comp
glslc.exe -w %include% -x glsl -o ../binary\\LightCulling.comp.spv LightCulling.comp
glslc.exe -w %include% -x glsl -o ../binary\\PMRendering.vert.spv PMRendering.vert
glslc.exe -w %include% -x glsl -o ../binary\\PMRendering.frag.spv PMRendering.frag

glslc.exe -w %include% -x glsl -o ../binary\\MakeDistanceField.comp.spv MakeDistanceField.comp
glslc.exe -w %include% -x glsl -o ../binary\\MakeDistanceField2.comp.spv MakeDistanceField2.comp
glslc.exe -w %include% -x glsl -o ../binary\\MakeDistanceField3.comp.spv MakeDistanceField3.comp
glslc.exe -w %include% -x glsl -o ../binary\\DebugRenderSDF.comp.spv DebugRenderSDF.comp


glslc.exe -w %include% -x glsl -o ../binary/PMPointLight.vert.spv PMPointLight.vert
glslc.exe -w %include% -x glsl -o ../binary/PMPointLight.frag.spv PMPointLight.frag

glslc.exe -w %include% -x glsl -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp


