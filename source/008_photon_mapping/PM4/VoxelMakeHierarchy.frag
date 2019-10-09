#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require

#include </convertDimension.glsl>
#include </Shape.glsl>
#include </PMDefine.glsl>
#include </Voxel.glsl>
#include </PM.glsl>

layout(binding=0, r32ui) uniform readonly uimage3D tVoxelAlbedoCount;
layout(binding=1, r32f) uniform readonly image3D tVoxelAlbedoR;
layout(binding=2, r32f) uniform readonly image3D tVoxelAlbedoG;
layout(binding=3, r32f) uniform readonly image3D tVoxelAlbedoB;
//layout(binding=4, r32f) uniform readonly image3D tVoxelNormalX;
//layout(binding=5, r32f) uniform readonly image3D tVoxelNormalY;
//layout(binding=6, r32f) uniform readonly image3D tVoxelNormalZ;
//layout(binding=7, r32ui) uniform uimage3D tVoxelAlbedoCountDest;
//layout(binding=8, r32f) uniform image3D tVoxelAlbedoRDest;
//layout(binding=9, r32f) uniform image3D tVoxelAlbedoGDest;
//layout(binding=10, r32f) uniform image3D tVoxelAlbedoBDest;
//layout(binding=11, r32f) uniform image3D tVoxelNormalXDest;
//layout(binding=12, r32f) uniform image3D tVoxelNormalYDest;
//layout(binding=13, r32f) uniform image3D tVoxelNormalZDest;
layout(binding=4, r32ui) uniform uimage3D tVoxelAlbedoCountDest;
layout(binding=5, r32f) uniform image3D tVoxelAlbedoRDest;
layout(binding=6, r32f) uniform image3D tVoxelAlbedoGDest;
layout(binding=7, r32f) uniform image3D tVoxelAlbedoBDest;

void main()
{	
	
	uint count = imageLoad(tVoxelAlbedoCount, index).r;
	if(count == 0)
	{
		discard;
	}

	vec3 normal = vec3(0.);
	vec3 albedo = vec3(imageLoad(tVoxelAlbedoR, index).r, imageLoad(tVoxelAlbedoG, index).r, imageLoad(tVoxelAlbedoB, index).r);
//		normal = vec3(imageLoad(tVoxelNormalX, index).r, imageLoad(tVoxelNormalY, index).r, imageLoad(tVoxelNormalZ, index).r);
	albedo /= count;
	normal /= count;

	ivec3 upIndex = index / 2;
	imageAtomicAdd(tVoxelAlbedoRDest, upIndex, albedo.r);
	imageAtomicAdd(tVoxelAlbedoGDest, upIndex, albedo.g);
	imageAtomicAdd(tVoxelAlbedoBDest, upIndex, albedo.b);
//	imageAtomicAdd(tVoxelNormalXDest, upIndex, normal.x);
//	imageAtomicAdd(tVoxelNormalYDest, upIndex, normal.y);
//	imageAtomicAdd(tVoxelNormalZDest, upIndex, normal.z);
	imageAtomicAdd(tVoxelAlbedoCountDest, upIndex, 1);

}