@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../ -I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary/

%exe% %include% %option% -o %output%/Fluid_CalcPressure.comp.spv Fluid_CalcPressure.comp
rem %exe% %include% %option% -o %output%/Fluid_CalcJoint.comp.spv Fluid_CalcJoint.comp
%exe% %include% %option% -o %output%/Fluid_Update.comp.spv Fluid_Update.comp
%exe% %include% %option% -o %output%/Fluid_ToFragment.comp.spv Fluid_ToFragment.comp

rem %exe% %include% %option% -o %output%/Softbody_CalcCenter.comp.spv Softbody_CalcCenter.comp
rem %exe% %include% %option% -o %output%/Softbody_CalcCenter_Post.comp.spv Softbody_CalcCenter_Post.comp
rem %exe% %include% %option% -o %output%/Softbody_CalcForce.comp.spv Softbody_CalcForce.comp
