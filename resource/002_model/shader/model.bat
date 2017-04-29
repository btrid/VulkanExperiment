@echo off
cd /d %~dp0

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\Clear.comp.spv Clear.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\AnimationUpdate.comp.spv AnimationUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\BoneTransform.comp.spv BoneTransform.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\MotionUpdate.comp.spv MotionUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\NodeTransform.comp.spv NodeTransform.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\CameraCulling.comp.spv CameraCulling.comp
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag

