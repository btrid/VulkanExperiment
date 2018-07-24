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
%exe% %include% %option% -o %output%/Fluid_CalcPressureMinimum.comp.spv Fluid_CalcPressureMinimum.comp
%exe% %include% %option% -o %output%/Fluid_CalcPressureGradient.comp.spv Fluid_CalcPressureGradient.comp
%exe% %include% %option% -o %output%/Fluid_CalcViscosity.comp.spv Fluid_CalcViscosity.comp
%exe% %include% %option% -o %output%/Fluid_Move1.comp.spv Fluid_Move1.comp
%exe% %include% %option% -o %output%/Fluid_Move2.comp.spv Fluid_Move2.comp
%exe% %include% %option% -o %output%/Fluid_Collision.comp.spv Fluid_Collision.comp
%exe% %include% %option% -o %output%/Fluid_CollisionAfter.comp.spv Fluid_CollisionAfter.comp

%exe% %include% %option% -o %output%/Fluid_ToFragment.comp.spv Fluid_ToFragment.comp

