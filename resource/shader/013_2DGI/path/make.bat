@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I./ -I../ -I../../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=-o ../../binary/

%exe% %include% %option% %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap.comp
%exe% %include% %option% %output%/GI2DPath_MakeReachMap_Precompute.comp.spv GI2DPath_MakeReachMap_Precompute.comp

%exe% %include% %option% %output%/Path_BuildTree.comp.spv Path_BuildTree.comp
%exe% %include% %option% %output%/Path_BuildTreeNode.comp.spv Path_BuildTreeNode.comp
%exe% %include% %option% %output%/Path_DrawTree.comp.spv Path_DrawTree.comp
