@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I./ -I../ -I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary/

%exe% %include% %option% -o %output%/Crowd_UnitUpdate.comp.spv Crowd_UnitUpdate.comp
%exe% %include% %option% -o %output%/Crowd_CalcWorldMatrix.comp.spv Crowd_CalcWorldMatrix.comp