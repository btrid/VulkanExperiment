@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include 
SET output=-o ../binary/

%exe% %output%/Boolean_Rendering.comp.spv Boolean_Rendering.comp
%exe% %output%/Boolean_MakeLDC.comp.spv Boolean_MakeLDC.comp
