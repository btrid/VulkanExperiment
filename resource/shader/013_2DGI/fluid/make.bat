@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Fluid_CalcMinimumPressure.comp.spv Fluid_CalcMinimumPressure.comp
%exe% %include% %option% -o %output%/Fluid_CalcPressure.comp.spv Fluid_CalcPressure.comp
%exe% %include% %option% -o %output%/Fluid_CalcPressureGradient.comp.spv Fluid_CalcPressureGradient.comp
%exe% %include% %option% -o %output%/Fluid_CalcViscosity.comp.spv Fluid_CalcViscosity.comp
%exe% %include% %option% -o %output%/Fluid_Move.comp.spv Fluid_Move.comp
%exe% %include% %option% -o %output%/Fluid_MoveBefore.comp.spv Fluid_MoveBefore.comp
%exe% %include% %option% -o %output%/Fluid_Collision.comp.spv Fluid_Collision.comp
%exe% %include% %option% -o %output%/Fluid_CollisionAfter.comp.spv Fluid_CollisionAfter.comp


