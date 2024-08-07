
@echo off
cd /d %~dp0
SET include=-I../ -I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary/

%exe% %include% %option% -o %output%/001_Clear.comp.spv 001_Clear.comp
%exe% %include% %option% -o %output%/002_AnimationUpdate.comp.spv 002_AnimationUpdate.comp
%exe% %include% %option% -o %output%/003_MotionUpdate.comp.spv 003_MotionUpdate.comp
%exe% %include% %option% -o %output%/004_NodeTransform.comp.spv 004_NodeTransform.comp
%exe% %include% %option% -o %output%/005_CameraCulling.comp.spv 005_CameraCulling.comp
%exe% %include% %option% -o %output%/006_BoneTransform.comp.spv 006_BoneTransform.comp

%exe% %include% %option% -o %output%/Render.vert.spv Render.vert
%exe% %include% %option% -o %output%/Render.frag.spv Render.frag
%exe% %include% %option% -o %output%/RenderFowardPlus.frag.spv RenderFowardPlus.frag
