@echo off
cd /d %~dp0
SET Include=-I Include -I ../../btrlib/shader/include 
SET output=binary

glslc.exe -w %Include%	-o ../binary\\ModelRender.vert.spv ModelRender.vert
glslc.exe -w %Include%	-o ../binary\\ModelRender.frag.spv ModelRender.frag

