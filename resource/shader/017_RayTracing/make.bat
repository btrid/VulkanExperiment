@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/VoxelMakeTD_ModelVoxelize.vert.spv VoxelMakeTD_ModelVoxelize.vert
%exe% %output%/VoxelMakeTD_ModelVoxelize.geom.spv VoxelMakeTD_ModelVoxelize.geom
%exe% %output%/VoxelMakeTD_ModelVoxelize.frag.spv VoxelMakeTD_ModelVoxelize.frag

%exe% %output%/VoxelMakeTD_AllocateTopChild.comp.spv VoxelMakeTD_AllocateTopChild.comp
%exe% %output%/VoxelMakeTD_AllocateMidChild.comp.spv VoxelMakeTD_AllocateMidChild.comp

%exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_2.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_2_fast.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_3_fast.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_3.comp

%exe% %output%/VoxelDebug_Render.vert.spv VoxelDebug_Render.vert
%exe% %output%/VoxelDebug_Render.frag.spv VoxelDebug_Render.frag
%exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_Render.geom

%exe% %output%/VoxelDebug_RenderHash.geom.spv VoxelDebug_RenderHash.geom
%exe% %output%/VoxelDebug_RenderTop.geom.spv VoxelDebug_RenderTop.geom
%exe% %output%/VoxelDebug_RenderMid.geom.spv VoxelDebug_RenderMid.geom
