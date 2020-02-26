@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/ -I./
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=-o ../binary

%exe% %include% %option% %output%/SkyReference.comp.spv SkyReference.comp
%exe% %include% %option% %output%/Sky_Render.comp.spv Sky_Render.comp
%exe% %include% %option% %output%/Sky_RenderUpsampling.comp.spv Sky_RenderUpsampling.comp
rem %exe% %include% %option% %output%/Sky_CalcDensity.comp.spv Sky_CalcDensity.comp
%exe% %include% %option% -DLSX=64 -DLSY=1 -DLSZ=1 %output%/Sky_MakeTexture_PartialX.comp.spv Sky_MakeTexture_Partial.comp
%exe% %include% %option% -DLSX=4 -DLSY=16 -DLSZ=1 %output%/Sky_MakeTexture_PartialY.comp.spv Sky_MakeTexture_Partial.comp
%exe% %include% %option% -DLSX=1 -DLSY=1 -DLSZ=64 %output%/Sky_MakeTexture_PartialZ.comp.spv Sky_MakeTexture_Partial.comp

%exe% %include% %option% %output%/Sky_Render2.comp.spv Sky_Render2.comp
%exe% %include% %option% %output%/Sky_MakeTexture2.comp.spv Sky_MakeTexture2.comp

%exe% %include% %option% %output%/SkyAriseReference.comp.spv SkyAriseReference.comp
%exe% %include% %option% %output%/SkyArise.comp.spv SkyArise.comp
%exe% %include% %option% %output%/SkyArise_MakeTexture.comp.spv SkyArise_MakeTexture.comp

%exe% %include% %option% %output%/WorleyNoise_Precompute.comp.spv WorleyNoise_Precompute.comp
%exe% %include% %option% %output%/WorleyNoise_Compute.comp.spv WorleyNoise_Compute.comp

rem %exe% %include% %option% %output%/SkyWithTexture.comp.spv Sky.comp


