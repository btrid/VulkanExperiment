@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/Render.mesh.spv Render.mesh
%exe% %output%/Render.task.spv Render.task
%exe% %output%/Render.frag.spv Render.frag

