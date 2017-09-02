@echo off
cd /d %~dp0

glslc.exe -w -I Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=vertex		-o ../binary\\DrawPrimitive.vert.spv DrawPrimitive.vert
glslc.exe -w -I Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o ../binary\\DrawPrimitive.frag.spv DrawPrimitive.frag
