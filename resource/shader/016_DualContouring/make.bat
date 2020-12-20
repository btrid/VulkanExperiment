@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include 
SET output=-o ../binary/

%exe% %output%/LDC_Construct.rgen.spv LDC_Construct.rgen
%exe% %output%/LDC_Construct.rmiss.spv LDC_Construct.rmiss
%exe% %output%/LDC_Construct.rchit.spv LDC_Construct.rchit

%exe% %output%/DC_MakeLDCCell.comp.spv DC_MakeLDCCell.comp
%exe% %output%/DC_MakeDCVertex.comp.spv DC_MakeDCVertex.comp
%exe% %output%/DC_MakeDCVFace.comp.spv DC_MakeDCVFace.comp

%exe% %output%/DC_Rendering.vert.spv DC_Rendering.vert
%exe% %output%/DC_Rendering.geom.spv DC_Rendering.geom
%exe% %output%/DC_Rendering.frag.spv DC_Rendering.frag
%exe% %output%/DC_RenderModel.vert.spv DC_RenderModel.vert
%exe% %output%/DC_RenderModel.frag.spv DC_RenderModel.frag

%exe% %output%/DCDebug_Rendering.vert.spv DCDebug_Rendering.vert
%exe% %output%/DCDebug_Rendering.geom.spv DCDebug_Rendering.geom
%exe% %output%/DCDebug_Rendering.frag.spv DCDebug_Rendering.frag
