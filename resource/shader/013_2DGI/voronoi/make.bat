@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../../include/
SET option=--target-env vulkan1.1 -V -w
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Voronoi_SetupJFA.comp.spv Voronoi_SetupJFA.comp
%exe% %include% %option% -o %output%/Voronoi_MakeJFA.comp.spv Voronoi_MakeJFA.comp
%exe% %include% %option% -o %output%/Voronoi_MakeTriangle.comp.spv Voronoi_MakeTriangle.comp
%exe% %include% %option% -o %output%/Voronoi_SortTriangle.comp.spv Voronoi_SortTriangle.comp
%exe% %include% %option% -o %output%/Voronoi_MakePath.comp.spv Voronoi_MakePath.comp

%exe% %include% %option% -o %output%/Voronoi_DestructWall.vert.spv Voronoi_DestructWall.vert
%exe% %include% %option% -o %output%/Voronoi_DestructWall.geom.spv Voronoi_DestructWall.geom
%exe% %include% %option% -o %output%/Voronoi_DestructWall.frag.spv Voronoi_DestructWall.frag

%exe% %include% %option% -o %output%/Voronoi_Draw.comp.spv Voronoi_Draw.comp
%exe% %include% %option% -o %output%/VoronoiDebug_DrawVoronoiTriangle.vert.spv VoronoiDebug_DrawVoronoiTriangle.vert
%exe% %include% %option% -o %output%/VoronoiDebug_DrawVoronoiTriangle.geom.spv VoronoiDebug_DrawVoronoiTriangle.geom
%exe% %include% %option% -o %output%/VoronoiDebug_DrawVoronoiTriangle.frag.spv VoronoiDebug_DrawVoronoiTriangle.frag

%exe% %include% %option% -o %output%/VoronoiDebug_DrawVoronoiPath.geom.spv VoronoiDebug_DrawVoronoiPath.geom



