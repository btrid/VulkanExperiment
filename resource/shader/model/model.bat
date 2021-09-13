
@echo off
cd /d %~dp0
SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/001_Clear.comp.spv 001_Clear.comp
%exe% %output%/002_AnimationUpdate.comp.spv 002_AnimationUpdate.comp
%exe% %output%/003_MotionUpdate.comp.spv 003_MotionUpdate.comp
%exe% %output%/004_NodeTransform.comp.spv 004_NodeTransform.comp
%exe% %output%/005_CameraCulling.comp.spv 005_CameraCulling.comp
%exe% %output%/006_BoneTransform.comp.spv 006_BoneTransform.comp

%exe% %output%/Render.vert.spv Render.vert
%exe% %output%/Render.frag.spv Render.frag
%exe% %output%/RenderFowardPlus.frag.spv RenderFowardPlus.frag
