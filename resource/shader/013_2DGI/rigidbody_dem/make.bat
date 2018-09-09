@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../ -I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Rigid_CalcPressure.comp.spv Rigid_CalcPressure.comp
%exe% %include% %option% -o %output%/Rigid_Update.comp.spv Rigid_Update.comp
%exe% %include% %option% -o %output%/Rigid_ToFragment.comp.spv Rigid_ToFragment.comp
%exe% %include% %option% -o %output%/Rigid_Integrate.comp.spv Rigid_Integrate.comp
