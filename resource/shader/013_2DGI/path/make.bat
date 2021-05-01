@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET exe=glslangValidator.exe -I../../include/ -I./ --target-env vulkan1.1 -V -g
SET output=-o ../../binary/


%exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap.comp
%exe% %include% %option% %output%/GI2DPath_MakeReachMap_Precompute.comp.spv GI2DPath_MakeReachMap_Precompute.comp
%exe% %include% %option% %output%/GI2DPath_DrawReachMap.comp.spv GI2DPath_DrawReachMap.comp

