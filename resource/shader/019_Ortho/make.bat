@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I../include -g
SET output=-o ../binary/

%exe% %output%/Ortho_Render.vert.spv Ortho_Render.vert
%exe% %output%/Ortho_Render.frag.spv Ortho_Render.frag
