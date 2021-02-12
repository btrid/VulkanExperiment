@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include 
SET output=-o ../binary/

%exe% %output%/Voxel_MakeBottom.comp.spv Voxel_MakeBottom.comp
%exe% %output%/Voxel_MakeTop.comp.spv Voxel_MakeTop.comp

%exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Top.comp
rem %exe% %output%/Voxel_Rendering.comp.spv Voxel_Rendering_Bottom.comp
