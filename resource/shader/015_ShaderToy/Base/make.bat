@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/ -I./
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=-o ../../binary

%exe% %include% %option% %output%/Sky_Render.comp.spv Sky_Render.comp
%exe% %include% %option% -DLSX=64 -DLSY=1 -DLSZ=1 %output%/Sky_MakeTexture_PartialX.comp.spv Sky_MakeTexture.comp
%exe% %include% %option% -DLSX=4 -DLSY=16 -DLSZ=1 %output%/Sky_MakeTexture_PartialY.comp.spv Sky_MakeTexture.comp
%exe% %include% %option% -DLSX=1 -DLSY=1 -DLSZ=64 %output%/Sky_MakeTexture_PartialZ.comp.spv Sky_MakeTexture.comp

rem %exe% %include% %option% %output%/SkyWithTexture.comp.spv Sky.comp


