@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=-o ../binary

%exe% %include% %option% %output%/Sky.comp.spv Sky.comp
%exe% %include% %option% %output%/SkyWithTexture.comp.spv SkyWithTexture.comp



