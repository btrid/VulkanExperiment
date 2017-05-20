@echo off
cd /d %~dp0

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\ParticleEmit.comp.spv ParticleEmit.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\ParticleUpdate.comp.spv ParticleUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\Render.vert.spv Render.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\Render.frag.spv Render.frag
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\FloorRender.vert.spv FloorRender.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\FloorRender.frag.spv FloorRender.frag
glslc.exe -w -I Include -x glsl -fshader-stage=geometry	-o binary\\FloorRender.geom.spv FloorRender.geom
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\MakeTriangleLL.vert.spv MakeTriangleLL.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\MakeTriangleLL.frag.spv MakeTriangleLL.frag
glslc.exe -w -I Include -x glsl -fshader-stage=geometry	-o binary\\MakeTriangleLL.geom.spv MakeTriangleLL.geom

