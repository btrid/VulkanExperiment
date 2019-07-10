@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=../binary

%exe% %include% %option% -o %output%/GI2DModel_MakeFragment.vert.spv GI2DModel_MakeFragment.vert
%exe% %include% %option% -o %output%/GI2DModel_MakeFragment.frag.spv GI2DModel_MakeFragment.frag

%exe% %include% %option% -o %output%/GI2D_MakeFragmentMap.comp.spv GI2D_MakeFragmentMap.comp
%exe% %include% %option% -o %output%/GI2D_MakeFragmentMapAndSDF.comp.spv GI2D_MakeFragmentMapAndSDF.comp
%exe% %include% %option% -o %output%/GI2D_MakeFragmentMapHierarchy.comp.spv GI2D_MakeFragmentMapHierarchy.comp

%exe% %include% %option% -o %output%/GI2DSDF_MakeJFA.comp.spv GI2DSDF_MakeJFA.comp
rem %exe% %include% %option% -o %output%/GI2DSDF_MakeJFA.comp.spv GI2DSDF_MakeJFA2.comp
%exe% %include% %option% -o %output%/GI2DSDF_MakeJFA_EX.comp.spv GI2DSDF_MakeJFA_EX.comp
rem %exe% %include% %option% -o %output%/GI2DSDF_MakeJFA_EX.comp.spv GI2DSDF_MakeJFA_EX2.comp
%exe% %include% %option% -o %output%/GI2DSDF_MakeSDF.comp.spv GI2DSDF_MakeSDF.comp
%exe% %include% %option% -o %output%/GI2DSDF_RenderSDF.comp.spv GI2DSDF_RenderSDF.comp

%exe% %include% %option% -o %output%/GI2DPath_MakeReachMap.comp.spv GI2DPath_MakeReachMap.comp
%exe% %include% %option% -o %output%/GI2DPath_MakeReachMap_Precompute.comp.spv GI2DPath_MakeReachMap_Precompute.comp

%exe% %include% %option% -o %output%/GI2DDebug_DrawFragment.comp.spv GI2DDebug_DrawFragment.comp
%exe% %include% %option% -o %output%/GI2DDebug_DrawFragmentMap.comp.spv GI2DDebug_DrawFragmentMap.comp
%exe% %include% %option% -o %output%/GI2DDebug_MakeLight.comp.spv GI2DDebug_MakeLight.comp
%exe% %include% %option% -o %output%/GI2DDebug_DrawReachMap.comp.spv GI2DDebug_DrawReachMap.comp

%exe% %include% %option% -o %output%/Radiosity_MakeVertex.comp.spv Radiosity_MakeVertex.comp
%exe% %include% %option% -o %output%/Radiosity_RayMarch.comp.spv Radiosity_RayMarch.comp
%exe% %include% %option% -o %output%/Radiosity_RayBounce.comp.spv Radiosity_RayBounce.comp

%exe% %include% %option% -o %output%/Radiosity_Radiosity.vert.spv Radiosity_Radiosity.vert
rem %exe% %include% %option% -o %output%/Radiosity_Render.geom.spv Radiosity_Render.geom
rem %exe% %include% %option% -o %output%/Radiosity_Render.frag.spv Radiosity_Render.frag
%exe% %include% %option% -o %output%/Radiosity_Radiosity.geom.spv Radiosity_RadiosityLine.geom
%exe% %include% %option% -o %output%/Radiosity_Radiosity.frag.spv Radiosity_RadiosityLine.frag

%exe% %include% %option% -o %output%/Radiosity_Rendering.vert.spv Radiosity_Rendering.vert
%exe% %include% %option% -o %output%/Radiosity_Rendering.frag.spv Radiosity_Rendering.frag

%exe% %include% %option% -o %output%/Radiosity3_Precompute.comp.spv Radiosity3_Precompute.comp
%exe% %include% %option% -o %output%/Radiosity3_Compute.comp.spv Radiosity3_Compute.comp
%exe% %include% %option% -o %output%/Radiosity3_Rendering.comp.spv Radiosity3_Rendering.comp


