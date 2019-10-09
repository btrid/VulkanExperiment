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
#extension GL_NV_shader_atomic_float : require
#include </convertDimension.glsl>
#include </convert_vec4_to_int.glsl>
#include </Shape.glsl>
#include </PMDefine.glsl>
#include </Voxel.glsl>
#include </PM.glsl>

layout(binding=0, r32f) uniform image3D tVoxelAlbedoR;
layout(binding=1, r32f) uniform image3D tVoxelAlbedoG;
layout(binding=2, r32f) uniform image3D tVoxelAlbedoB;
layout(binding=3, r32f) uniform image3D tVoxelNormalX;
layout(binding=4, r32f) uniform image3D tVoxelNormalY;
layout(binding=5, r32f) uniform image3D tVoxelNormalZ;
layout(binding=6, r32ui) uniform uimage3D tVoxelAlbedoCount;


layout(std430, binding = 0) buffer MaterialBuffer
{
	Material bMaterial[];
};


in FSIN
{
	vec3 Position;
	vec3 Normal;
	vec4 Texcoord;
	flat int MaterialIndex;
}FSIn;

layout(location = 0) out vec4 FragColor;

void main()
{
	
	ivec3 index = getVoxelIndex(FSIn.Position.xyz);
	{
		Material mat = bMaterial[FSIn.MaterialIndex];
		vec3 albedo = getDiffuse(mat, FSIn.Texcoord.xy);
#if 1
		imageAtomicAdd(tVoxelAlbedoR, index, albedo.r);
		imageAtomicAdd(tVoxelAlbedoG, index, albedo.g);
		imageAtomicAdd(tVoxelAlbedoB, index, albedo.b);
		imageAtomicAdd(tVoxelNormalX, index, FSIn.Normal.x);
		imageAtomicAdd(tVoxelNormalY, index, FSIn.Normal.y);
		imageAtomicAdd(tVoxelNormalZ, index, FSIn.Normal.z);
		imageAtomicAdd(tVoxelAlbedoCount, index, 1);
#else
		int index1D = convert3DTo1D(index, ivec3(BRICK_SUB_SIZE));
//		int index1D = 5;
		atomicAdd(bVoxel[index1D].albedo.r, albedo.r);
		atomicAdd(bVoxel[index1D].albedo.g, albedo.g);
		atomicAdd(bVoxel[index1D].albedo.b, albedo.b);
		atomicAdd(bVoxel[index1D].normal.x, FSIn.Normal.x);
		atomicAdd(bVoxel[index1D].normal.y, FSIn.Normal.y);
		atomicAdd(bVoxel[index1D].normal.z, FSIn.Normal.z);
		atomicAdd(bVoxel[index1D].count, 1);
#endif
	}
}