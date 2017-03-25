@echo off
cd /d %~dp0

glslc.exe -w -I Include -x glsl -fshader-stage=compute -o Clear.comp.spv Clear.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute -o AnimationUpdate.comp.spv AnimationUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute -o BoneTransform.comp.spv BoneTransform.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute -o MotionUpdate.comp.spv MotionUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute -o NodeTransform.comp.spv NodeTransform.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute -o CameraCulling.comp.spv CameraCulling.comp
glslc.exe -w -I Include -x glsl -fshader-stage=vertex -o Render.vert.spv Render.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment -o Render.frag.spv Render.frag

