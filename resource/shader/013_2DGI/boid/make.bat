@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Boid_CalcPressure.comp.spv Boid_CalcPressure.comp
%exe% %include% %option% -o %output%/Boid_Move_Grid.comp.spv Boid_Move_Grid.comp
%exe% %include% %option% -o %output%/Boid_ToFragment.comp.spv Boid_ToFragment.comp

