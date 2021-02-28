@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/ModelVoxelize.vert.spv ModelVoxelize.vert
%exe% %output%/ModelVoxelize.geom.spv ModelVoxelize.geom
%exe% %output%/ModelVoxelize.frag.spv ModelVoxelize.frag

%exe% %output%/Voxel_AllocateTopChild.comp.spv Voxel_AllocateTopChild.comp
%exe% %output%/Voxel_AllocateMidChild.comp.spv Voxel_AllocateMidChild.comp

 %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering.comp

%exe% %output%/VoxelDebug_Render.vert.spv VoxelDebug_Render.vert
%exe% %output%/VoxelDebug_Render.frag.spv VoxelDebug_Render.frag

%exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderHash.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderTop.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeaf.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeafChild.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderMid.geom
