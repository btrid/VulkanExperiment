@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/ModelVoxelize.vert.spv ModelVoxelize.vert
%exe% %output%/ModelVoxelize.geom.spv ModelVoxelize.geom
%exe% %output%/ModelVoxelize_0.frag.spv ModelVoxelize_0.frag
%exe% %output%/ModelVoxelize_1.frag.spv ModelVoxelize_1.frag
%exe% %output%/ModelVoxelize_2.frag.spv ModelVoxelize_2.frag
%exe% %output%/ModelVoxelize_data.frag.spv ModelVoxelize_data.frag

%exe% %output%/Voxel_Allocate.comp.spv Voxel_Allocate.comp
%exe% %output%/Voxel_AllocateTop.comp.spv Voxel_AllocateTop.comp
%exe% %output%/Voxel_AllocateTopChild.comp.spv Voxel_AllocateTopChild.comp
%exe% %output%/Voxel_AllocateMid.comp.spv Voxel_AllocateMid.comp

 %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering.comp

%exe% %output%/VoxelDebug_Render.vert.spv VoxelDebug_Render.vert
%exe% %output%/VoxelDebug_Render.frag.spv VoxelDebug_Render.frag

rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderTop.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeaf.geom
 %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderLeafChild.geom
rem %exe% %output%/VoxelDebug_Render.geom.spv VoxelDebug_RenderMid.geom
