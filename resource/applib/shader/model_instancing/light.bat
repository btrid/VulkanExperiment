@echo off
cd /d %~dp0

glslc.exe -w -I ../Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=compute	-o ../binary\\CullLight.comp.spv Light\\CullLight.comp
rem glslc.exe -w -I ../Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=compute	-o ../binary\\MakeFrustom.comp.spv Light\\MakeFrustom.comp
rem glslc.exe -w -I ../Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o ../binary\\Render.vert.spv Render.vert
rem glslc.exe -w -I ../Include -I ../../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o ../binary\\Render.frag.spv Render.frag

