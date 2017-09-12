#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#ifdef VULKAN
#extension GL_GOOGLE_cpp_style_line_directive : require
#else
#extension GL_ARB_shading_language_include : require
#endif

#include </convertDimension.glsl>

#define SETPOINT_VOXEL 0
#include </Voxelize.glsl>

layout(location=1)in Transform{
	vec3 Position;
}transform;

layout(push_constant) uniform ConstantBlock
{
	layout(offset=0) uint u_material_index;
} constant;

void main()
{
	
	ivec3 index = getVoxelIndex(transform.Position.xyz);
	{
//		Material mat = bMaterial[constant.u_material_index];
//		vec3 albedo = getDiffuse(mat, transform.Texcoord.xy);

//		imageAtomicAdd(tVoxelAlbedoR, index, albedo.r);
//		imageAtomicAdd(tVoxelAlbedoG, index, albedo.g);
//		imageAtomicAdd(tVoxelAlbedoB, index, albedo.b);
//		imageAtomicAdd(tVoxelAlbedoCount, index, 1);

	}

}