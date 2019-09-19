@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=-o ../../binary/

%exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap3.comp
%exe% %include% %option% %output%/GI2DPath_MakeReachMap_Precompute.comp.spv GI2DPath_MakeReachMap2_Precompute.comp
