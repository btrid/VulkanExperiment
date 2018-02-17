@echo off
cd /d %~dp0

SET header=-I Include -I ../../../btrlib/shader/include -I ../../../applib/shader/include 

glslc.exe -w %header% -x glsl -fshader-stage=vertex		-o ../binary\\DrawPrimitive.vert.spv DrawPrimitive.vert
glslc.exe -w %header% -x glsl -fshader-stage=fragment	-o ../binary\\DrawPrimitive.frag.spv DrawPrimitive.frag
