@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I../include -g
SET output=-o ../binary/

%exe% %output%/SN_Render.vert.spv SN_Render.vert
%exe% %output%/SN_Render.frag.spv SN_Render.frag
