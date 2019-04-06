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
rem %exe% %include% %option% -o %output%/Rigid_CollisionDetective.comp.spv Rigid_CollisionDetective.comp
rem %exe% %include% %option% -o %output%/Rigid_CollisionDetectiveBefore.comp.spv Rigid_CollisionDetectiveBefore.comp
%exe% %include% %option% -o %output%/Rigid_ConstraintMake.comp.spv Rigid_ConstraintMake.comp
%exe% %include% %option% -o %output%/Rigid_ConstraintSolve.comp.spv Rigid_ConstraintSolve.comp
%exe% %include% %option% -o %output%/Rigid_ConstraintIntegrate.comp.spv Rigid_ConstraintIntegrate.comp

%exe% %include% %option% -o %output%/Rigid_CalcForce.comp.spv Rigid_CalcForce.comp
%exe% %include% %option% -o %output%/Rigid_IntegrateParticle.comp.spv Rigid_IntegrateParticle.comp
%exe% %include% %option% -o %output%/Rigid_Integrate.comp.spv Rigid_Integrate.comp
%exe% %include% %option% -o %output%/Rigid_UpdateRigidbody.comp.spv Rigid_UpdateRigidbody.comp

%exe% %include% %option% -o %output%/Rigid_ToFluid.comp.spv Rigid_ToFluid.comp
%exe% %include% %option% -o %output%/Rigid_ToFluidWall.comp.spv Rigid_ToFluidWall.comp

%exe% %include% %option% -o %output%/Rigid_MakeParticle.comp.spv Rigid_MakeParticle.comp
%exe% %include% %option% -o %output%/Rigid_MakeFluid.comp.spv Rigid_MakeFluid.comp

%exe% %include% %option% -o %output%/Rigid_ShapeMatchingMakeParticle.comp.spv Rigid_ShapeMatchingMakeParticle.comp
%exe% %include% %option% -o %output%/Rigid_ShapeMatchingCalcCenterMass.comp.spv Rigid_ShapeMatchingCalcCenterMass.comp
%exe% %include% %option% -o %output%/Rigid_ShapeMatchingApqAccum.comp.spv Rigid_ShapeMatchingApqAccum.comp
%exe% %include% %option% -o %output%/Rigid_ShapeMatchingApqCalc.comp.spv Rigid_ShapeMatchingApqCalc.comp
%exe% %include% %option% -o %output%/Rigid_ShapeMatchingIntegrate.comp.spv Rigid_ShapeMatchingIntegrate.comp



