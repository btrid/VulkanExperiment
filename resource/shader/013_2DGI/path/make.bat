@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I./ -I../ -I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../../binary/

%exe% %include% %option% -o %output%/Path_BuildTree.comp.spv Path_BuildTree.comp
%exe% %include% %option% -o %output%/Path_Precompute.comp.spv Path_Precompute.comp
%exe% %include% %option% -o %output%/Path_PathFinding.comp.spv Path_PathFinding.comp
