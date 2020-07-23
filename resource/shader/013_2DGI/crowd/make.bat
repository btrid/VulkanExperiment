@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary/

%exe% %include% %option% -o %output%/Crowd_UnitUpdate.comp.spv Crowd_UnitUpdate.comp
rem %exe% %include% %option% -o %output%/Crowd_MakeDensityMap.comp.spv Crowd_MakeDensityMap.comp
rem %exe% %include% %option% -o %output%/Crowd_DrawDensityMap.comp.spv Crowd_DrawDensityMap.comp
rem %exe% %include% %option% -o %output%/Crowd_MakeLinkList.comp.spv Crowd_MakeLinkList.comp
%exe% %include% %option% -o %output%/Crowd_Render.comp.spv Crowd_Render.comp
