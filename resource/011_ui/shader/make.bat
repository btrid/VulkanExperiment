@echo off
cd /d %~dp0
SET btrlib=../../btrlib/shader/include
SET include=-I Include -I ../../btrlib/shader/include -I ../../applib/shader/include
glslc.exe -w -I Include -I %btrlib% -x glsl -fshader-stage=compute	-o binary\\UIAnimation.comp.spv UIAnimation.comp
glslc.exe -w -I Include -I %btrlib% -x glsl -fshader-stage=compute	-o binary\\UIUpdate.comp.spv UIUpdate.comp
glslc.exe -w -I Include -I %btrlib% -x glsl -fshader-stage=compute	-o binary\\UITransform.comp.spv UITransform.comp
glslc.exe -w %include% -x glsl -fshader-stage=compute	-o binary\\UIBoundary.comp.spv UIBoundary.comp
glslc.exe -w -I Include -I %btrlib% -x glsl -fshader-stage=vertex	-o binary\\UIRender.vert.spv UIRender.vert
glslc.exe -w -I Include -I %btrlib% -x glsl -fshader-stage=fragment	-o binary\\UIRender.frag.spv UIRender.frag


