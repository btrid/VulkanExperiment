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
%exe% %include% %option% %output%/SkyMakeTexture.comp.spv SkyMakeTexture.comp
rem %exe% %include% %option% -DLSX=64 -DLSY=1 -DLSZ=1 %output%/SkyMakeTexture_PartialX.comp.spv SkyMakeTexture_Partial.comp
rem %exe% %include% %option% -DLSX=4 -DLSY=16 -DLSZ=1 %output%/SkyMakeTexture_PartialY.comp.spv SkyMakeTexture_Partial.comp
rem %exe% %include% %option% -DLSX=1 -DLSY=1 -DLSZ=64 %output%/SkyMakeTexture_PartialZ.comp.spv SkyMakeTexture_Partial.comp
%exe% %include% %option% %output%/SkyMakeTexture_Partial.comp.spv SkyMakeTexture_Partial.comp
%exe% %include% %option% %output%/SkyArise.comp.spv SkyArise.comp
%exe% %include% %option% %output%/SkyArise_MakeTexture.comp.spv SkyArise_MakeTexture.comp

rem %exe% %include% %option% %output%/SkyWithTexture.comp.spv Sky.comp


