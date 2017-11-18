@echo off
cd /d %~dp0

glslc.exe -w -I Include -I ../../btrlib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\PlaySound.comp.spv PlaySound.comp
