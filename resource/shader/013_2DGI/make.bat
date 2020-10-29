@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=-o ../binary

%exe% %include% %option% %output%/GI2DModel_MakeFragment.vert.spv GI2DModel_MakeFragment.vert
%exe% %include% %option% %output%/GI2DModel_MakeFragment.frag.spv GI2DModel_MakeFragment.frag

%exe% %include% %option% %output%/GI2D_MakeFragmentMap.comp.spv GI2D_MakeFragmentMap.comp
%exe% %include% %option% %output%/GI2D_MakeFragmentMapAndSDF.comp.spv GI2D_MakeFragmentMapAndSDF.comp
%exe% %include% %option% %output%/GI2D_MakeFragmentMapHierarchy.comp.spv GI2D_MakeFragmentMapHierarchy.comp

%exe% %include% %option% %output%/GI2DDebug_DrawFragment.comp.spv GI2DDebug_DrawFragment.comp
%exe% %include% %option% %output%/GI2DDebug_DrawFragmentMap.comp.spv GI2DDebug_DrawFragmentMap.comp
%exe% %include% %option% %output%/GI2DDebug_MakeLight.comp.spv GI2DDebug_MakeLight.comp
rem %exe% %include% %option% %output%/GI2DDebug_DrawReachMap.comp.spv GI2DDebug_DrawReachMap_fast.comp
%exe% %include% %option% %output%/GI2DDebug_DrawReachMap.comp.spv GI2DDebug_DrawReachMap.comp



