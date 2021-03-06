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

rem %exe% %include% %option% -o ../binary\\RayMarch.comp.spv RayMarch_1.comp
rem %exe% %include% %option% -o ../binary\\RayCollect.comp.spv RayCollect_1.comp
%exe% %include% %option% -o ../binary\\RayMarch.comp.spv RayMarch_64.comp
%exe% %include% %option% -o ../binary\\RayCollect.comp.spv RayCollect_64.comp

%exe% %include% %option% -o ../binary\\MakeFragmentMap.comp.spv MakeFragmentMap.comp
%exe% %include% %option% -o ../binary\\MakeFragmentMapHierarchy.comp.spv MakeFragmentMapHierarchy.comp
%exe% %include% %option% -o ../binary\\LightCulling.comp.spv LightCulling.comp
%exe% %include% %option% -o ../binary\\RayMarchRendering.vert.spv RayMarchRendering.vert
%exe% %include% %option% -o ../binary\\RayMarchRendering.frag.spv RayMarchRendering.frag

%exe% %include% %option% -o ../binary/GI2DDebugLight.comp.spv GI2DDebugLight.comp

%exe% %include% %option% -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp

%exe% %include% %option% -o ../binary\\RTCompute.comp.spv RTCompute.comp
%exe% %include% %option% -o ../binary\\RTRendering.comp.spv RTRendering.comp
rem %exe% %include% %option% -o ../binary\\RTRendering.comp.spv RTRenderingDebug.comp
%exe% %include% %option% -o ../binary\\RTRendering.vert.spv RTRendering.vert
%exe% %include% %option% -o ../binary\\RTRendering.frag.spv RTRendering.frag

%exe% %include% %option% -o ../binary\\PhotonMapping.comp.spv PhotonMapping_64.comp
%exe% %include% %option% -o ../binary\\PhotonCollect.comp.spv PhotonCollect_64.comp

%exe% %include% %option% -o ../binary\\Radiosity.comp.spv Radiosity_64.comp
%exe% %include% %option% -o ../binary\\Radiosity_Clear.comp.spv Radiosity_Clear.comp
%exe% %include% %option% -o ../binary\\Radiosity_MakeBounceMap.comp.spv Radiosity_MakeBounceMap.comp
%exe% %include% %option% -o ../binary\\Radiosity_Bounce.comp.spv Radiosity_Bounce.comp
%exe% %include% %option% -o ../binary\\Radiosity_Render.frag.spv Radiosity_Render.frag
