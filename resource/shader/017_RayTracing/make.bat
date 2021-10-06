@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/VoxelMake_ModelVoxelize.vert.spv VoxelMake_ModelVoxelize.vert
%exe% %output%/VoxelMake_ModelVoxelize.geom.spv VoxelMake_ModelVoxelize.geom
%exe% %output%/VoxelMake_ModelVoxelize.frag.spv VoxelMake_ModelVoxelize.frag

%exe% %output%/VoxelMake_AllocateTopChild.comp.spv VoxelMake_AllocateTopChild.comp
%exe% %output%/VoxelMake_AllocateMidChild.comp.spv VoxelMake_AllocateMidChild.comp
%exe% %output%/VoxelMake_MakeHashMapMask.comp.spv VoxelMake_MakeHashMapMask.comp

 %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering.comp
 %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_2.comp
 %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_2_fast.comp
%exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_3_fast.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_3.comp

%exe% %output%/VoxelDebug_Render.vert.spv VoxelDebug_Render.vert
%exe% %output%/VoxelDebug_Render.frag.spv VoxelDebug_Render.frag
%exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_Render.geom

rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderTop.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderTopChild.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderMid.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeaf.geom
%exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeafChild.geom
