@echo off
cd /d %~dp0

glslc.exe -w -I ../Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o ../binary\\ModelRender.vert.spv ModelRender.vert
glslc.exe -w -I ../Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o ../binary\\ModelRender.frag.spv ModelRender.frag

