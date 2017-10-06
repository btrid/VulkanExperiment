@echo off
cd /d %~dp0

rem glslc.exe -w -I Include -x glsl -fshader-stage=compute	-o binary\\ParticleEmit.comp.spv ParticleEmit.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\ParticleGenerate.comp.spv ParticleGenerate.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\ParticleGenerateDebug.comp.spv ParticleGenerateDebug.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=compute	-o binary\\ParticleUpdate.comp.spv ParticleUpdate.comp
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=vertex	-o binary\\ParticleRender.vert.spv ParticleRender.vert
glslc.exe -w -I Include -I ../../applib/shader/include -I ../../btrlib/shader/include -x glsl -fshader-stage=fragment	-o binary\\ParticleRender.frag.spv ParticleRender.frag