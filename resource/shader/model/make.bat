@echo off
cd /d %~dp0

SET include=-I ../Include 

glslc.exe -w %include% -o ../binary\\001_Clear.comp.spv 001_Clear.comp
glslc.exe -w %include% -o ../binary\\002_AnimationUpdate.comp.spv 002_AnimationUpdate.comp
glslc.exe -w %include% -o ../binary\\003_MotionUpdate.comp.spv 003_MotionUpdate.comp
glslc.exe -w %include% -o ../binary\\004_NodeTransform.comp.spv 004_NodeTransform.comp
glslc.exe -w %include% -o ../binary\\005_CameraCulling.comp.spv 005_CameraCulling.comp
glslc.exe -w %include% -o ../binary\\006_BoneTransform.comp.spv 006_BoneTransform.comp
glslc.exe -w %include% -o ../binary\\Render.vert.spv Render.vert
glslc.exe -w %include% -o ../binary\\Render.frag.spv Render.frag
glslc.exe -w %include% -o ../binary\\RenderFowardPlus.frag.spv RenderFowardPlus.frag


