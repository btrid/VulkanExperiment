@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe

%exe% %include% %option% -o ../../binary/MakeFragmentAppModel.vert.spv MakeFragmentAppModel.vert
%exe% %include% %option% -o ../../binary/MakeFragmentAppModel.frag.spv MakeFragmentAppModel.frag

%exe% %include% %option% -o ../../binary/LightCulling.comp.spv LightCulling.comp

%exe% %include% %option% -o ../../binary/PMPointLight.comp.spv PMPointLight.comp
%exe% %include% %option% -o ../../binary/MakeShadowVolume.comp.spv MakeShadowVolume.comp


%exe% %include% %option% -o ../../binary/DrawShadowVolume.vert.spv DrawShadowVolume.vert
%exe% %include% %option% -o ../../binary/DrawShadowVolume.frag.spv DrawShadowVolume.frag