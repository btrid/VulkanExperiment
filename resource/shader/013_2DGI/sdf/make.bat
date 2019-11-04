@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/ 
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=-o ../../binary

%exe% %include% %option% %output%/GI2DSDF_MakeJFA.comp.spv GI2DSDF_MakeJFA.comp
rem %exe% %include% %option% %output%/GI2DSDF_MakeJFA.comp.spv GI2DSDF_MakeJFA2.comp
%exe% %include% %option% %output%/GI2DSDF_MakeJFA_EX.comp.spv GI2DSDF_MakeJFA_EX.comp
rem %exe% %include% %option% %output%/GI2DSDF_MakeJFA_EX.comp.spv GI2DSDF_MakeJFA_EX2.comp
%exe% %include% %option% %output%/GI2DSDF_MakeSDF.comp.spv GI2DSDF_MakeSDF.comp
%exe% %include% %option% %output%/GI2DSDF_RenderSDF.comp.spv GI2DSDF_RenderSDF.comp




