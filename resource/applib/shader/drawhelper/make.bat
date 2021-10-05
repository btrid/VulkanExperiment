@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I../../../btrlib/shader/include -I../../../applib/shader/include -g
SET output=-o ../binary/

%exe% %output%/DrawPrimitive.vert.spv DrawPrimitive.vert
%exe% %output%/DrawPrimitive.frag.spv DrawPrimitive.frag

%exe% %output%/DrawTex.vert.spv DrawTex.vert
%exe% %output%/DrawTex.frag.spv DrawTex.frag

