@echo off
cd /d %~dp0

glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\DrawPrimitive.vert.spv DrawPrimitive.vert
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\DrawPrimitive.frag.spv DrawPrimitive.frag

rem glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\Volume.comp.spv Volume.comp
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\VolumeRender.vert.spv VolumeRender.vert
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\VolumeRender.frag.spv VolumeRender.frag
rem glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\002_AnimationUpdate.comp.spv 002_AnimationUpdate.comp
rem glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\003_MotionUpdate.comp.spv 003_MotionUpdate.comp
rem glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\004_NodeTransform.comp.spv 004_NodeTransform.comp
rem glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\005_CameraCulling.comp.spv 005_CameraCulling.comp
rem glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\006_BoneTransform.comp.spv 006_BoneTransform.comp
rem glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
rem glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag
rem glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\RenderFowardPlus.frag.spv RenderFowardPlus.frag


