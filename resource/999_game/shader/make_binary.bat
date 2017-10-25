@echo off
cd /d %~dp0

rem glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\ParticleEmit.comp.spv ParticleEmit.comp

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\FloorRender.vert.spv FloorRender.vert
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\FloorRender.frag.spv FloorRender.frag
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=geometry	-o binary\\FloorRender.geom.spv FloorRender.geom
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\FloorRenderEx.vert.spv FloorRenderEx.vert
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\FloorRenderEx.frag.spv FloorRenderEx.frag
glslc.exe -w -I Include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\MapVoxelize.comp.spv MapVoxelize.comp

rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\MakeTriangleLL.vert.spv MakeTriangleLL.vert
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\MakeTriangleLL.frag.spv MakeTriangleLL.frag
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=geometry	-o binary\\MakeTriangleLL.geom.spv MakeTriangleLL.geom
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\RenderBrick.vert.spv RenderBrick.vert
rem glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\RenderBrick.frag.spv RenderBrick.frag

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\BoidSoldierEmit.comp.spv BoidSoldierEmit.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\BoidSoldierUpdate.comp.spv BoidSoldierUpdate.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\BoidBrainUpdate.comp.spv BoidBrainUpdate.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\BoidSoldierRender.vert.spv BoidSoldierRender.vert
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\BoidSoldierRender.frag.spv BoidSoldierRender.frag

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\BulletUpdate.comp.spv BulletUpdate.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\BulletEmit.comp.spv BulletEmit.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\BulletRender.vert.spv BulletRender.vert
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\BulletRender.frag.spv BulletRender.frag

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\CollisionTest.comp.spv CollisionTest.comp

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\FloorUpdateDamage.comp.spv FloorUpdateDamage.comp

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex		-o binary\\ModelRender.vert.spv ModelRender.vert
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\ModelRender.frag.spv ModelRender.frag

glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\LightTileCulling.comp.spv LightTileCulling.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\LightCollectBullet.comp.spv LightCollectBullet.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\LightCollectParticle.comp.spv LightCollectParticle.comp
