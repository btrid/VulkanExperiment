@echo off
cd /d %~dp0

glslangvalidator -w -V Clear.comp -o Clear.comp.spv
glslangvalidator -w -V CameraCulling.comp -o CameraCulling.comp.spv
glslangvalidator -w -V AnimationUpdate.comp -o AnimationUpdate.comp.spv
glslangvalidator -w -V BoneTransform.comp -o BoneTransform.comp.spv
glslangvalidator -w -V MotionUpdate.comp -o MotionUpdate.comp.spv
glslangvalidator -w -V NodeTransform.comp -o NodeTransform.comp.spv
glslangvalidator -w -V Render.vert -o Render.vert.spv
glslangvalidator -w -V Render.frag -o Render.frag.spv

