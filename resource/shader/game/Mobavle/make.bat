@echo off
cd /d %~dp0

SET include=-I../../include/
SET option=--target-env vulkan1.1 -V -w
SET exe=glslangValidator.exe
SET output=-o ../../binary

%exe% %include% %option% %output%/Movable_UpdatePrePhysics.comp.spv Movable_UpdatePrePhysics.comp
%exe% %include% %option% %output%/Movable_UpdatePostPhysics.comp.spv Movable_UpdatePostPhysics.comp



