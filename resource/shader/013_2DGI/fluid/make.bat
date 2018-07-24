@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Fluid_CalcPressure.comp.spv Fluid_CalcPressure.comp
%exe% %include% %option% -o %output%/Fluid_Update.comp.spv Fluid_Update.comp
%exe% %include% %option% -o %output%/Fluid_ToFragment.comp.spv Fluid_ToFragment.comp

