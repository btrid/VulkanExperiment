@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../ -I../../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Rigid_ToFragment.comp.spv Rigid_ToFragment.comp
%exe% %include% %option% -o %output%/Rigid_ToFluidWall.comp.spv Rigid_ToFluidWall.comp
%exe% %include% %option% -o %output%/Rigid_ToFluidWall2.comp.spv Rigid_ToFluidWall2.comp

%exe% %include% %option% -o %output%/Rigid_MakeParticle.comp.spv Rigid_MakeParticle.comp
%exe% %include% %option% -o %output%/Rigid_MakeFluid.comp.spv Rigid_MakeFluid.comp
%exe% %include% %option% -o %output%/Rigid_ConstraintSolve.comp.spv Rigid_ConstraintSolve.comp
%exe% %include% %option% -o %output%/Rigid_CalcCenterMass.comp.spv Rigid_CalcCenterMass.comp
%exe% %include% %option% -o %output%/Rigid_ApqAccum.comp.spv Rigid_ApqAccum.comp
%exe% %include% %option% -o %output%/Rigid_ApqCalc.comp.spv Rigid_ApqCalc.comp

%exe% %include% %option% -o %output%/RigidMake_MakeJFA.comp.spv RigidMake_MakeJFA.comp
%exe% %include% %option% -o %output%/RigidMake_MakeSDF.comp.spv RigidMake_MakeSDF.comp




