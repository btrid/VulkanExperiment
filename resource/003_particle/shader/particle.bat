@echo off
cd /d %~dp0

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\ParticleEmit.comp.spv ParticleEmit.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\ParticleUpdate.comp.spv ParticleUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag

