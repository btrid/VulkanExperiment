@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/Voxel_MakeBottom.comp.spv Voxel_MakeBottom.comp
%exe% %output%/Voxel_MakeTop.comp.spv Voxel_MakeTop.comp

%exe% %output%/Voxel_Render.vert.spv Voxel_Render.vert
%exe% %output%/Voxel_Render.geom.spv Voxel_Render.geom
%exe% %output%/Voxel_Render.frag.spv Voxel_Render.frag

%exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Top.comp
%exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Bottom.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Bottom2.comp
%exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Bottom3.comp
