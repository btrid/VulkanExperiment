@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe

%exe% %include% %option% -o ../binary\\MakeFragmentAppModel.vert.spv MakeFragmentAppModel.vert
%exe% %include% %option% -o ../binary\\MakeFragmentAppModel.frag.spv MakeFragmentAppModel.frag

rem %exe% %include% %option% -o ../binary\\PhotonMapping.comp.spv PhotonMapping_1.comp
rem %exe% %include% %option% -o ../binary\\PhotonCollect.comp.spv PhotonCollect_1.comp
%exe% %include% %option% -o ../binary\\PhotonMapping.comp.spv PhotonMapping_64.comp
rem %exe% %include% %option% -o ../binary\\PhotonMapping.comp.spv PhotonMapping_64_2.comp 遅い
%exe% %include% %option% -o ../binary\\PhotonCollect.comp.spv PhotonCollect_64.comp

%exe% %include% %option% -o ../binary\\MakeFragmentMap.comp.spv MakeFragmentMap.comp
%exe% %include% %option% -o ../binary\\MakeFragmentMapHierarchy.comp.spv MakeFragmentMapHierarchy.comp
%exe% %include% %option% -o ../binary\\LightCulling.comp.spv LightCulling.comp
%exe% %include% %option% -o ../binary\\PMRendering.vert.spv PMRendering.vert
%exe% %include% %option% -o ../binary\\PMRendering.frag.spv PMRendering.frag

%exe% %include% %option% -o ../binary/PMPointLight.comp.spv PMPointLight.comp
%exe% %include% %option% -o ../binary/PMMakeLightList.comp.spv PMMakeLightList.comp

%exe% %include% %option% -o ../binary/MakeDistanceField.comp.spv MakeDistanceField.comp

%exe% %include% %option% -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp

