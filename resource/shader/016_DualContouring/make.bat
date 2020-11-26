@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ 
SET output=-o ../binary/

%exe% %output%/LDC_Construct.rgen.spv LDC_Construct.rgen
%exe% %output%/LDC_Construct.rmiss.spv LDC_Construct.rmiss
%exe% %output%/LDC_Construct.rchit.spv LDC_Construct.rchit

%exe% %output%/DC_MakeLDCCell.comp.spv DC_MakeLDCCell.comp
