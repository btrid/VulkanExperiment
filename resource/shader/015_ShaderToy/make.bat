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

%exe% %include% %option% %output%/Sky_Precompute_Shadow.comp.spv Sky_Precompute_Shadow.comp
rem %exe% %include% %option% %output%/Sky_Precompute_PowderEffect.comp.spv Sky_Precompute_PowderEffect.comp

%exe% %include% %option% %output%/Sky_Render.comp.spv Sky_Render.comp
%exe% %include% %option% %output%/Sky_RenderUpsampling.comp.spv Sky_RenderUpsampling.comp

%exe% %include% %option% %output%/Sky_RenderShadow.comp.spv Sky_RenderShadow.comp


%exe% %include% %option% %output%/WorleyNoise_Compute.comp.spv WorleyNoise_Compute.comp
%exe% %include% %option% %output%/WorleyNoise_ComputeWeatherTexture.comp.spv WorleyNoise_ComputeWeatherTexture.comp
%exe% %include% %option% %output%/WorleyNoise_ComputeDistortTexture.comp.spv WorleyNoise_ComputeDistortTexture.comp
%exe% %include% %option% %output%/WorleyNoise_Render.comp.spv WorleyNoise_Render.comp

rem %exe% %include% %option% %output%/SkyWithTexture.comp.spv Sky.comp