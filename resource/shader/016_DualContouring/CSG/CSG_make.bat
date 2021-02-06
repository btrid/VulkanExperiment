@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../ -I../../include 
SET output=-o ../../binary/

rem %exe% %output%/Boolean_Rendering.comp.spv Boolean_Rendering.comp
%exe% %output%/LayeredDepth_BooleanAdd.comp.spv LayeredDepth_BooleanAdd.comp
%exe% %output%/LayeredDepth_BooleanSub.comp.spv LayeredDepth_BooleanSub.comp
rem %exe% %output%/LayeredDepth_MakeDepthMap.comp.spv LayeredDepth_MakeDepthMap.comp
%exe% %output%/LayeredDepth_MakeDepthMap.vert.spv LayeredDepth_MakeDepthMap.vert
%exe% %output%/LayeredDepth_MakeDepthMap.frag.spv LayeredDepth_MakeDepthMap.frag

%exe% %output%/CSG_RenderModel.vert.spv CSG_RenderModel.vert
%exe% %output%/CSG_RenderModel.frag.spv CSG_RenderModel.frag
