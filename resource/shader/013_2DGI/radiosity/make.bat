@echo off
cd /d %~dp0
SET include=-I../../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=-o ../../binary

%exe% %include% %option% %output%/Radiosity_MakeVertex.comp.spv Radiosity_MakeVertex.comp
%exe% %include% %option% %output%/Radiosity_RayMarch.comp.spv Radiosity_RayMarch.comp
%exe% %include% %option% %output%/Radiosity_RayBounce.comp.spv Radiosity_RayBounce.comp

%exe% %include% %option% %output%/Radiosity_Radiosity.vert.spv Radiosity_Radiosity.vert
%exe% %include% %option% %output%/Radiosity_Radiosity.geom.spv Radiosity_Radiosity.geom
%exe% %include% %option% %output%/Radiosity_Radiosity.frag.spv Radiosity_Radiosity.frag

%exe% %include% %option% %output%/Radiosity_Rendering.vert.spv Radiosity_Rendering.vert
%exe% %include% %option% %output%/Radiosity_Rendering.frag.spv Radiosity_Rendering.frag

%exe% %include% %option% %output%/Radiosity_MakeDirectLight.comp.spv Radiosity_MakeDirectLight.comp
%exe% %include% %option% %output%/Radiosity_DirectLighting.vert.spv Radiosity_DirectLighting.vert
%exe% %include% %option% %output%/Radiosity_DirectLighting.frag.spv Radiosity_DirectLighting.frag

%exe% %include% %option% %output%/Radiosity_PixelBasedRaytracing.comp.spv Radiosity_PixelBasedRaytracing.comp
%exe% %include% %option% %output%/Radiosity_PixelBasedRaytracing2.comp.spv Radiosity_PixelBasedRaytracing2.comp
%exe% %include% %option% %output%/Radiosity_PixelBasedRaytracing2.comp.spv Radiosity_PixelBasedRaytracing2.1.comp

