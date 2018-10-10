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

%exe% %include% %option% -o %output%/GI2D_MakeFragmentMap.comp.spv GI2D_MakeFragmentMap.comp

%exe% %include% %option% -o %output%/GI2D_MakeJFA.comp.spv GI2D_MakeJFA.comp
%exe% %include% %option% -o %output%/GI2D_MakeSDF.comp.spv GI2D_MakeSDF.comp
%exe% %include% %option% -o %output%/GI2D_RenderSDF.frag.spv GI2D_RenderSDF.frag

rem %exe% %include% %option% -o ../binary\\DebugFragmentMap.comp.spv DebugFragmentMap.comp


%exe% %include% %option% -o %output%/Radiosity_Clear.comp.spv Radiosity_Clear.comp

rem %exe% %include% %option% -o %output%/Radiosity_Render.frag.spv Radiosity_Render.frag
 %exe% %include% %option% -o %output%/Radiosity_Render.frag.spv Radiosity_Render_2.frag
%exe% %include% %option% -o %output%/Radiosity_Render.vert.spv Radiosity_Render.vert

rem %exe% %include% %option% -o %output%/Radiosity_CalcRadiance.comp.spv Radiosity_CalcRadiance.comp

%exe% %include% %option% -o %output%/Radiosity_RayGenerate.comp.spv Radiosity_RayGenerate.comp
%exe% %include% %option% -o %output%/Radiosity_RaySetup.comp.spv Radiosity_RaySetup.comp
%exe% %include% %option% -o %output%/Radiosity_RaySort.comp.spv Radiosity_RaySort.comp
%exe% %include% %option% -o %output%/Radiosity_RayMarch.comp.spv Radiosity_RayMarch.comp
%exe% %include% %option% -o %output%/Radiosity_RayHit.comp.spv Radiosity_RayHit.comp
%exe% %include% %option% -o %output%/Radiosity_RayBounce.comp.spv Radiosity_RayBounce.comp

%exe% %include% %option% -o %output%/Radiosity_RayMarch_Space.comp.spv Radiosity_RayMarch_Space.comp
%exe% %include% %option% -o %output%/Radiosity_RayMarch_Light.comp.spv Radiosity_RayMarch_Light.comp
%exe% %include% %option% -o %output%/Radiosity_SegmentSort.comp.spv Radiosity_SegmentSort.comp
%exe% %include% %option% -o %output%/Radiosity.comp.spv Radiosity.comp

rem SDF版（重い）
%exe% %include% %option% -o %output%/Radiosity_RayMarch_Space_SDF.comp.spv Radiosity_RayMarch_Space_SDF.comp
%exe% %include% %option% -o %output%/Radiosity_RayMarch_Light_SDF.comp.spv Radiosity_RayMarch_Light_SDF.comp
%exe% %include% %option% -o %output%/Radiosity2.comp.spv Radiosity2.comp

%exe% %include% %option% -o %output%/GI2D_DebugMakeLight.comp.spv GI2D_DebugMakeLight.comp

call fluid/make.bat
cd /d %~dp0
rem call rigidbody_dem/make.bat
rem cd /d %~dp0
