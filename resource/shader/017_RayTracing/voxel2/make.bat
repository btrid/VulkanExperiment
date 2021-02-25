@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../../include -g
SET output=-o ../../binary/

rem %exe% %output%/Voxel_MakeBottom.comp.spv Voxel_MakeBottom.comp
rem %exe% %output%/Voxel_MakeTop.comp.spv Voxel_MakeTop.comp

%exe% %output%/Voxel_Allocate.comp.spv Voxel_Allocate.comp
%exe% %output%/Voxel_AllocateTop.comp.spv Voxel_AllocateTop.comp
%exe% %output%/Voxel_AllocateTopChild.comp.spv Voxel_AllocateTopChild.comp
%exe% %output%/Voxel_AllocateMid.comp.spv Voxel_AllocateMid.comp

 %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Top.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Bottom.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Bottom2.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Bottom3.comp

%exe% %output%/VoxelDebug_Render.vert.spv VoxelDebug_Render.vert
%exe% %output%/VoxelDebug_Render.frag.spv VoxelDebug_Render.frag

rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderTop.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeaf.geom
 %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeafChild.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderMid.geom
