@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../binary

%exe% %include% %option% -o %output%/MakeFragmentAppModel.vert.spv MakeFragmentAppModel.vert
%exe% %include% %option% -o %output%/MakeFragmentAppModel.frag.spv MakeFragmentAppModel.frag

rem %exe% %include% %option% -o %output%/MakeFragmentMap.comp.spv MakeFragmentMap.comp
 %exe% %include% %option% -o %output%/MakeFragmentMap.comp.spv MakeFragmentMap_2.comp
%exe% %include% %option% -o %output%/MakeFragmentMapHierarchy.comp.spv MakeFragmentMapHierarchy.comp

%exe% %include% %option% -o %output%/GI2D_MakeDensityHierarchy.comp.spv GI2D_MakeDensityHierarchy.comp
%exe% %include% %option% -o %output%/GI2D_MakeLight.comp.spv GI2D_MakeLight.comp

rem %exe% %include% %option% -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp


rem %exe% %include% %option% -o %output%/Radiosity.comp.spv Radiosity_64.comp
rem %exe% %include% %option% -o %output%/Radiosity.comp.spv Radiosity_64_2.comp
rem %exe% %include% %option% -o %output%/Radiosity.comp.spv Radiosity_64_3.comp
rem %exe% %include% %option% -o %output%/Radiosity.comp.spv Radiosity_64_4.comp
%exe% %include% %option% -o %output%/Radiosity.comp.spv Radiosity_64_5.comp
rem %exe% %include% %option% -o %output%/Radiosity_Clear.comp.spv Radiosity_Clear.comp
%exe% %include% %option% -o %output%/Radiosity_Clear.comp.spv Radiosity_Clear_5.comp
%exe% %include% %option% -o %output%/Radiosity_MakeBounceMap.comp.spv Radiosity_MakeBounceMap.comp
%exe% %include% %option% -o %output%/Radiosity_Bounce.comp.spv Radiosity_Bounce.comp
rem %exe% %include% %option% -o %output%/Radiosity_Render.frag.spv Radiosity_Render.frag
%exe% %include% %option% -o %output%/Radiosity_Render.frag.spv Radiosity_Render_5.frag
%exe% %include% %option% -o %output%/Radiosity_Render.vert.spv Radiosity_Render.vert

%exe% %include% %option% -o %output%/Radiosity_CalcRadiance.comp.spv Radiosity_CalcRadiance.comp
%exe% %include% %option% -o %output%/Radiosity2.comp.spv Radiosity2.comp

%exe% %include% %option% -o %output%/Radiosity_RayGenerate.comp.spv Radiosity_RayGenerate.comp
%exe% %include% %option% -o %output%/Radiosity_RaySort.comp.spv Radiosity_RaySort.comp

call fluid/make.bat
cd /d %~dp0
call rigidbody_dem/make.bat
cd /d %~dp0
