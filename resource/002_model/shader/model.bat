@echo off
cd /d %~dp0

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\001_Clear.comp.spv 001_Clear.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\002_AnimationUpdate.comp.spv 002_AnimationUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\003_MotionUpdate.comp.spv 003_MotionUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\004_NodeTransform.comp.spv 004_NodeTransform.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\005_CameraCulling.comp.spv 005_CameraCulling.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\006_BoneTransform.comp.spv 006_BoneTransform.comp
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag


