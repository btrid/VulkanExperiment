@echo off
cd /d %~dp0
SET include=-I../ -I../../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Radiosity_MakeVertex.comp.spv Radiosity_MakeVertex.comp
%exe% %include% %option% -o %output%/Radiosity_RayMarch.comp.spv Radiosity_RayMarch.comp
%exe% %include% %option% -o %output%/Radiosity_RayBounce.comp.spv Radiosity_RayBounce.comp

%exe% %include% %option% -o %output%/Radiosity_Radiosity.vert.spv Radiosity_Radiosity.vert
rem %exe% %include% %option% -o %output%/Radiosity_Render.geom.spv Radiosity_Render.geom
rem %exe% %include% %option% -o %output%/Radiosity_Render.frag.spv Radiosity_Render.frag
%exe% %include% %option% -o %output%/Radiosity_Radiosity.geom.spv Radiosity_RadiosityLine.geom
%exe% %include% %option% -o %output%/Radiosity_Radiosity.frag.spv Radiosity_RadiosityLine.frag

%exe% %include% %option% -o %output%/Radiosity_Rendering.vert.spv Radiosity_Rendering.vert
%exe% %include% %option% -o %output%/Radiosity_Rendering.frag.spv Radiosity_Rendering.frag

