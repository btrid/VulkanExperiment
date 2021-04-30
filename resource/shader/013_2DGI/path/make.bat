@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET exe=glslangValidator.exe -I../../include/ -I./ --target-env vulkan1.1 -V -g
SET output=-o ../../binary/

%exe% %output%/GI2DPath_MakeReachableMap_Precompute.comp.spv GI2DPath_MakeReachableMap_Precompute.comp
%exe% %output%/GI2DPath_MakeReachableMap.comp.spv GI2DPath_MakeReachableMap.comp
%exe% %output%/GI2DDebug_DrawReachableMap.comp.spv GI2DDebug_DrawReachableMap.comp

%exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap.comp
%exe% %include% %option% %output%/GI2DPath_MakeReachMap_Precompute.comp.spv GI2DPath_MakeReachMap_Precompute.comp
%exe% %include% %option% %output%/GI2DDebug_DrawReachMap.comp.spv GI2DDebug_DrawReachMap.comp

rem %exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap1_old.comp
rem %exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap2.comp
%exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap_Stable.comp
%exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap_Stable_Table.comp

 %exe% %include% %option% %output%/GI2DPath_MakeReachMap_Begin.comp.spv GI2DPath_MakeReachMap_Begin.comp
 %exe% %include% %option% %output%/GI2DPath_MakeReachMap_Loop.comp.spv GI2DPath_MakeReachMap_Loop.comp
rem %exe% %include% %option% %output%/GI2DPath_MakeReachMap_End.comp.spv GI2DPath_MakeReachMap_End.comp
