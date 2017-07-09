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
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\RenderBrick.vert.spv RenderBrick.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\RenderBrick.frag.spv RenderBrick.frag

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\BoidSoldierEmit.comp.spv BoidSoldierEmit.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\BoidSoldierUpdate.comp.spv BoidSoldierUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\BoidBrainUpdate.comp.spv BoidBrainUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\BoidSoldierRender.vert.spv BoidSoldierRender.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\BoidSoldierRender.frag.spv BoidSoldierRender.frag

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\BulletUpdate.comp.spv BulletUpdate.comp
glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\BulletEmit.comp.spv BulletEmit.comp
glslc.exe -w -I Include -x glsl -fshader-stage=vertex	-o binary\\BulletRender.vert.spv BulletRender.vert
glslc.exe -w -I Include -x glsl -fshader-stage=fragment	-o binary\\BulletRender.frag.spv BulletRender.frag

glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\CollisionTest.comp.spv CollisionTest.comp
