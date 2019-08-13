@echo off
cd /d %~dp0
SET include=-I../../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Radiosity2_MakeVertex.comp.spv Radiosity3_MakeVertex.comp
%exe% %include% %option% -o %output%/Radiosity2_RayMarch.comp.spv Radiosity3_RayMarch.comp
%exe% %include% %option% -o %output%/Radiosity2_RayBounce.comp.spv Radiosity3_RayBounce.comp

%exe% %include% %option% -o %output%/Radiosity2_Radiosity.vert.spv Radiosity2_Radiosity.vert
%exe% %include% %option% -o %output%/Radiosity2_Radiosity.geom.spv Radiosity3_Radiosity.geom
%exe% %include% %option% -o %output%/Radiosity2_Radiosity.frag.spv Radiosity3_Radiosity.frag

%exe% %include% %option% -o %output%/Radiosity2_Rendering.vert.spv Radiosity2_Rendering.vert
%exe% %include% %option% -o %output%/Radiosity2_Rendering.comp.spv Radiosity2_Rendering.comp

