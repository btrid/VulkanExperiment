@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I../include -g
SET output=-o ../binary/

%exe% %output%/P_Render.vert.spv P_Render.vert
%exe% %output%/P_Render.frag.spv P_Render.frag

%exe% %output%/Wall_Render.vert.spv Wall_Render.vert
%exe% %output%/Wall_Render.geom.spv Wall_Render.geom
%exe% %output%/Wall_Render.frag.spv Wall_Render.frag
