@echo off
cd /d %~dp0
SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I../../../applib/shader/include -g
SET output=-o ../binary/

%exe% %output%/ImGuiRender.vert.spv ImGuiRender.vert
%exe% %output%/ImGuiRender.frag.spv ImGuiRender.frag


