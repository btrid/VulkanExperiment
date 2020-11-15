@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.1 -V -I./ 
SET output=-o ../binary/

%exe% %output%/DC_MakeVoxel.vert.spv DC_MakeVoxel.vert
%exe% %output%/DC_MakeVoxel.geom.spv DC_MakeVoxel.geom
%exe% %output%/DC_MakeVoxel.frag.spv DC_MakeVoxel.frag
