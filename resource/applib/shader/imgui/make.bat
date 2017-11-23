@echo off
cd /d %~dp0
SET btrlib=../../../btrlib/shader/include
SET output=../binary
glslc.exe -w -I Include -I %btrlib% -x glsl -fshader-stage=vertex	-o %output%\\ImGuiRender.vert.spv ImGuiRender.vert
glslc.exe -w -I Include -I %btrlib% -x glsl -fshader-stage=fragment	-o %output%\\ImGuiRender.frag.spv ImGuiRender.frag


