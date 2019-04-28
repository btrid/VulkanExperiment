@echo off
cd /d %~dp0
rem SET exe=glslc.exe
rem SET include=-I ../include
rem SET option=-w -x glsl
SET include=-I../ -I../../include/
SET option=--target-env spirv1.3 -V -w
SET exe=glslangValidator.exe
SET output=../../binary

%exe% %include% %option% -o %output%/Rigid_ToFragment.comp.spv Rigid_ToFragment.comp
%exe% %include% %option% -o %output%/Rigid_MakeCollidableWall.comp.spv Rigid_MakeCollidableWall.comp

%exe% %include% %option% -o %output%/Rigid_MakeParticle.comp.spv Rigid_MakeParticle.comp
%exe% %include% %option% -o %output%/Rigid_MakeCollidable.comp.spv Rigid_MakeCollidable.comp
%exe% %include% %option% -o %output%/Rigid_CollisionDetective.comp.spv Rigid_CollisionDetective.comp
%exe% %include% %option% -o %output%/Rigid_CollisionDetective_ray.comp.spv Rigid_CollisionDetective_ray.comp
%exe% %include% %option% -o %output%/Rigid_CalcCenterMass.comp.spv Rigid_CalcCenterMass.comp
%exe% %include% %option% -o %output%/Rigid_MakeTransformMatrix.comp.spv Rigid_MakeTransformMatrix.comp
%exe% %include% %option% -o %output%/Rigid_UpdateParticleBlock.comp.spv Rigid_UpdateParticleBlock.comp
%exe% %include% %option% -o %output%/Rigid_UpdateRigidbody.comp.spv Rigid_UpdateRigidbody.comp

%exe% %include% %option% -o %output%/RigidMake_Register.comp.spv RigidMake_Register.comp
%exe% %include% %option% -o %output%/RigidMake_MakeJFA.comp.spv RigidMake_MakeJFA.comp
%exe% %include% %option% -o %output%/RigidMake_MakeSDF.comp.spv RigidMake_MakeSDF.comp

%exe% %include% %option% -o %output%/Voronoi_Make.comp.spv Voronoi_Make.comp
%exe% %include% %option% -o %output%/Voronoi_MakeTriangle.comp.spv Voronoi_MakeTriangle.comp

%exe% %include% %option% -o %output%/Voronoi_Draw.comp.spv Voronoi_Draw.comp
%exe% %include% %option% -o %output%/Debug_DrawVoronoiTriangle.vert.spv Debug_DrawVoronoiTriangle.vert
%exe% %include% %option% -o %output%/Debug_DrawVoronoiTriangle.geom.spv Debug_DrawVoronoiTriangle.geom
%exe% %include% %option% -o %output%/Debug_DrawVoronoiTriangle.frag.spv Debug_DrawVoronoiTriangle.frag



