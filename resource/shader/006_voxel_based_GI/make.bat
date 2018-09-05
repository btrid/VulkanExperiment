@echo off
cd /d %~dp0

SET include=-I../include/ -I./Include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=binary

%exe% %include% %option% -o %output%/DrawVoxel.vert.spv DrawVoxel.vert
%exe% %include% %option% -o %output%/DrawVoxel.frag.spv DrawVoxel.frag

%exe% %include% %option% -o %output%/MakeVoxelHierarchy.comp.spv MakeVoxelHierarchy.comp

%exe% %include% %option% -o %output%/ModelVoxelize.vert.spv ModelVoxelize.vert
%exe% %include% %option% -o %output%/ModelVoxelize.geom.spv ModelVoxelize.geom
%exe% %include% %option% -o %output%/ModelVoxelize.frag.spv ModelVoxelize.frag

%exe% %include% %option% -o %output%/CopyVoxel.comp.spv CopyVoxel.comp
