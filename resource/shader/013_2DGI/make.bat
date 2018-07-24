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

%exe% %include% %option% -o ../binary\\MakeFragmentMap.comp.spv MakeFragmentMap.comp
%exe% %include% %option% -o ../binary\\MakeFragmentMapHierarchy.comp.spv MakeFragmentMapHierarchy.comp

rem %exe% %include% %option% -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp


%exe% %include% %option% -o ../binary\\Radiosity.comp.spv Radiosity_64.comp
%exe% %include% %option% -o ../binary\\Radiosity_Clear.comp.spv Radiosity_Clear.comp
%exe% %include% %option% -o ../binary\\Radiosity_MakeBounceMap.comp.spv Radiosity_MakeBounceMap.comp
%exe% %include% %option% -o ../binary\\Radiosity_Bounce.comp.spv Radiosity_Bounce.comp
%exe% %include% %option% -o ../binary\\Radiosity_Render.frag.spv Radiosity_Render.frag
%exe% %include% %option% -o ../binary\\Radiosity_Render.vert.spv Radiosity_Render.vert

call fluid/make.bat
cd /d %~dp0
