@echo off
cd /d %~dp0

SET exe=glslangValidator.exe --target-env vulkan1.2 -V -I./ -I../include -g
SET output=-o ../binary/

rem %exe% %output%/ModelRender.vert.spv ModelRender.vert
rem %exe% %output%/ModelRender.mesh.spv ModelRender.mesh
%exe% %output%/ModelRender2.task.spv ModelRender2.task
%exe% %output%/ModelRender2.mesh.spv ModelRender2.mesh
%exe% %output%/ModelRender.frag.spv ModelRender.frag

%exe% %output%/Skybox.vert.spv Skybox.vert
%exe% %output%/Skybox.geom.spv Skybox.geom
%exe% %output%/Skybox.frag.spv Skybox.frag

%exe% %output%/BRDFLUT_Make.vert.spv BRDFLUT_Make.vert
%exe% %output%/BRDFLUT_Make.frag.spv BRDFLUT_Make.frag

%exe% %output%/filtercube.vert.spv filtercube.vert
%exe% %output%/filtercube.geom.spv filtercube.geom
%exe% %output%/irradiancecube.frag.spv irradiancecube.frag
%exe% %output%/prefilterenvmap.frag.spv prefilterenvmap.frag
