@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../ -I../../include/ -I../sdf/
SET option=--target-env vulkan1.1 -V -w
SET exe=glslangValidator.exe
SET output=-o ../../binary

%exe% %include% %option% %output%/Rigid_ToFragment.comp.spv Rigid_ToFragment.comp
%exe% %include% %option% %output%/Rigid_DrawParticle.comp.spv Rigid_DrawParticle.comp
%exe% %include% %option% %output%/Rigid_MakeCollidableWall.comp.spv Rigid_MakeCollidableWall.comp

%exe% %include% %option% %output%/Rigid_MakeParticle.comp.spv Rigid_MakeParticle.comp
%exe% %include% %option% %output%/Rigid_MakeCollidable.comp.spv Rigid_MakeCollidable.comp
rem %exe% %include% %option% %output%/Rigid_CollisionDetective.comp.spv Rigid_CollisionDetective.comp
 %exe% %include% %option% %output%/Rigid_CollisionDetective.comp.spv Rigid_CollisionDetective2.comp
%exe% %include% %option% %output%/Rigid_CollisionDetective_Fluid.comp.spv Rigid_CollisionDetective_Fluid.comp
%exe% %include% %option% %output%/Rigid_CalcPressure.comp.spv Rigid_CalcPressure.comp
%exe% %include% %option% %output%/Rigid_CalcCenterMass.comp.spv Rigid_CalcCenterMass.comp
%exe% %include% %option% %output%/Rigid_MakeTransformMatrix.comp.spv Rigid_MakeTransformMatrix.comp
%exe% %include% %option% %output%/Rigid_UpdateParticleBlock.comp.spv Rigid_UpdateParticleBlock.comp
%exe% %include% %option% %output%/Rigid_UpdateRigidbody.comp.spv Rigid_UpdateRigidbody.comp

%exe% %include% %option% %output%/RigidMake_MakeJFA.comp.spv RigidMake_MakeJFA.comp
%exe% %include% %option% %output%/RigidMake_SetupRigidbody.comp.spv RigidMake_SetupRigidbody.comp
%exe% %include% %option% %output%/RigidMake_SetupParticle.comp.spv RigidMake_SetupParticle.comp



