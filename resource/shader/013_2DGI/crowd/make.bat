@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET exe=glslangValidator.exe --target-env vulkan1.1 -V -I../../include/ 
SET output=-o ../../binary/

%exe% %output%/Crowd_MakeLinkList.comp.spv Crowd_MakeLinkList.comp
%exe% %output%/Crowd_UnitUpdate.comp.spv Crowd_UnitUpdate.comp
rem %exe% %output%/Crowd_MakeDensityMap.comp.spv Crowd_MakeDensityMap.comp
rem %exe% %output%/Crowd_DrawDensityMap.comp.spv Crowd_DrawDensityMap.comp
%exe% %output%/Crowd_Render.comp.spv Crowd_Render.comp
