@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET exe=glslangValidator.exe -I../../include/ --target-env vulkan1.1 -V
SET output=-o ../../binary/

%exe% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap_fast.comp
%exe% %output%/GI2DPath_MakeReachMap_Precompute.comp.spv GI2DPath_MakeReachMap_fast_Precompute.comp
%exe% %output%/GI2DDebug_DrawReachMap.comp.spv GI2DDebug_DrawReachMap_fast.comp
