@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../binary

%exe% %include% %option% -o %output%/GI2DModel_MakeFragment.vert.spv GI2DModel_MakeFragment.vert
%exe% %include% %option% -o %output%/GI2DModel_MakeFragment.frag.spv GI2DModel_MakeFragment.frag

%exe% %include% %option% -o %output%/GI2D_MakeFragmentMap.comp.spv GI2D_MakeFragmentMap.comp

%exe% %include% %option% -o %output%/GI2D_MakeJFA.comp.spv GI2D_MakeJFA.comp
%exe% %include% %option% -o %output%/GI2D_MakeSDF.comp.spv GI2D_MakeSDF.comp
%exe% %include% %option% -o %output%/GI2D_RenderSDF.frag.spv GI2D_RenderSDF.frag

rem %exe% %include% %option% -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp


%exe% %include% %option% -o %output%/Radiosity_Clear.comp.spv Radiosity_Clear.comp

%exe% %include% %option% -o %output%/Radiosity_Render.frag.spv Radiosity_Render.frag
%exe% %include% %option% -o %output%/Radiosity_Render.vert.spv Radiosity_Render.vert

rem %exe% %include% %option% -o %output%/Radiosity_CalcRadiance.comp.spv Radiosity_CalcRadiance.comp

rem %exe% %include% %option% -o %output%/Radiosity_RayGenerate.comp.spv Radiosity_RayGenerate.comp
rem %exe% %include% %option% -o %output%/Radiosity_RayGenerate.comp.spv Radiosity_RayGenerate.1.comp
rem %exe% %include% %option% -o %output%/Radiosity_RayGenerate.comp.spv Radiosity_RayGenerate.3.comp
%exe% %include% %option% -o %output%/Radiosity_RayGenerate.comp.spv Radiosity_RayGenerate.4.comp
%exe% %include% %option% -o %output%/Radiosity_RaySetup.comp.spv Radiosity_RaySetup.comp
%exe% %include% %option% -o %output%/Radiosity_RaySort.comp.spv Radiosity_RaySort.comp
%exe% %include% %option% -o %output%/Radiosity_RayMarch.comp.spv Radiosity_RayMarch.comp
%exe% %include% %option% -o %output%/Radiosity_RayHit.comp.spv Radiosity_RayHit.comp
%exe% %include% %option% -o %output%/Radiosity_RayBounce.comp.spv Radiosity_RayBounce.comp

%exe% %include% %option% -o %output%/Radiosity.comp.spv Radiosity.comp

%exe% %include% %option% -o %output%/GI2D_DebugMakeLight.comp.spv GI2D_DebugMakeLight.comp

call fluid/make.bat
cd /d %~dp0
call crowd/make.bat
cd /d %~dp0
rem call rigidbody_dem/make.bat
rem cd /d %~dp0
