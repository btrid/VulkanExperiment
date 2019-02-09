@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../ -I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Rigid_ToFragment.comp.spv Rigid_ToFragment.comp
%exe% %include% %option% -o %output%/Rigid_Integrate.comp.spv Rigid_Integrate.comp
%exe% %include% %option% -o %output%/Rigid_CollisionDetective.comp.spv Rigid_CollisionDetective.comp
%exe% %include% %option% -o %output%/Rigid_CollisionDetectiveBefore.comp.spv Rigid_CollisionDetectiveBefore.comp
%exe% %include% %option% -o %output%/Rigid_CalcForce.comp.spv Rigid_CalcForce.comp
