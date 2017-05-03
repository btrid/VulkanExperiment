@echo off
cd /d %~dp0

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\CullLight.comp.spv Light\\CullLight.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\MakeFrustom.comp.spv Light\\MakeFrustom.comp
;glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
;glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag

