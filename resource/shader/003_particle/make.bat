@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I../include -g
SET output=-o ../binary/

%exe% %output%/P_Render.vert.spv P_Render.vert
%exe% %output%/P_Render.frag.spv P_Render.frag
