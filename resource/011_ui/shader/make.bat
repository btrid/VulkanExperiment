@echo off
cd /d %~dp0
SET btrlib=../../btrlib/shader/include
SET include=-I Include -I ../../btrlib/shader/include -I ../../applib/shader/include
glslc.exe -w %include% -x glsl -o binary\\UIAnimation.comp.spv UIAnimation.comp
glslc.exe -w %include% -x glsl -o binary\\UIClear.comp.spv UIClear.comp
glslc.exe -w %include% -x glsl -o binary\\UIUpdate.comp.spv UIUpdate.comp
glslc.exe -w %include% -x glsl -o binary\\UITransform.comp.spv UITransform.comp
glslc.exe -w %include% -x glsl -o binary\\UIBoundary.comp.spv UIBoundary.comp
glslc.exe -w %include% -x glsl -o binary\\UIRender.vert.spv UIRender.vert
glslc.exe -w %include% -x glsl -o binary\\UIRender.frag.spv UIRender.frag

glslc.exe -w %include% -x glsl -o binary\\FontRender.vert.spv FontRender.vert
glslc.exe -w %include% -x glsl -o binary\\FontRender.frag.spv FontRender.frag

glslc.exe -w %include% -x glsl -o binary\\FontRasterRender.vert.spv FontRasterRender.vert
glslc.exe -w %include% -x glsl -o binary\\FontRasterRender.frag.spv FontRasterRender.frag


