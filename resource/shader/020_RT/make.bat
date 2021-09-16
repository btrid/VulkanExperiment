@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/ModelRender.vert.spv ModelRender.vert
%exe% %output%/ModelRender.frag.spv ModelRender.frag
