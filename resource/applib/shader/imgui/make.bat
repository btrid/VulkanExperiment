@echo off
cd /d %~dp0
SET btrlib=../../../btrlib/shader/include
SET header=-I Include -I ../../../btrlib/shader/include -I ../../../applib/shader/include 

SET output=../binary
glslc.exe -w %header% -x glsl -fshader-stage=vertex	-o %output%\\ImGuiRender.vert.spv ImGuiRender.vert
glslc.exe -w %header% -x glsl -fshader-stage=fragment	-o %output%\\ImGuiRender.frag.spv ImGuiRender.frag


