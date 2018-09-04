@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/
SET option=--target-env vulkan1.1 -V
SET exe=glslangValidator.exe
SET output=../binary

%exe% %include% %option% -o %output%/PhotonMapping.comp.spv PhotonMapping.comp
%exe% %include% %option% -o %output%/PhotonMappingBounce.comp.spv PhotonMappingBounce.comp
%exe% %include% %option% -o %output%/PhotonRendering.comp.spv PhotonRendering.comp

%exe% %include% %option% -o %output%/PM_MakeVoxel.vert.spv PM_MakeVoxel.vert
%exe% %include% %option% -o %output%/PM_MakeVoxel.geom.spv PM_MakeVoxel.geom
%exe% %include% %option% -o %output%/PM_MakeVoxel.frag.spv PM_MakeVoxel.frag
