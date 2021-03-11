@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

%exe% %output%/Flowmap_Drop.comp.spv Flowmap_Drop.comp
%exe% %output%/Flowmap_Diffusion.comp.spv Flowmap_Diffusion.comp
%exe% %output%/Flowmap_Update.comp.spv Flowmap_Update.comp
%exe% %output%/Flowmap_Render.comp.spv Flowmap_Render.comp
rem %exe% %output%/Flowmap_Render.comp.spv Flowmap_Render_wind.comp
