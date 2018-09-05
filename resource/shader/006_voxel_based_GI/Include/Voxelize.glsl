#ifndef VOXELIZE_GLSL_
#define VOXELIZE_GLSL_

struct VoxelInfo
{
	vec4 u_area_min;
	vec4 u_area_max;
	vec4 u_cell_size;
	uvec4 u_cell_num;
};

#if defined(USE_VOXEL)
layout(std140, set=USE_VOXEL, binding = 0) restrict uniform VoxelInfoUniform
{
	VoxelInfo u_voxel_info;
};
layout (set=USE_VOXEL, binding=1, rgba32f) restrict uniform image3D t_voxel_image[8];
layout (set=USE_VOXEL, binding=2) uniform sampler3D t_voxel_sampler;
#endif

ivec3 getVoxelIndex(in VoxelInfo info, in vec3 pos) 
{
	return ivec3((pos - info.u_area_min.xyz) / info.u_cell_size.xyz);
}

#define denominator (512.)
//#define denominator (16.)
uint packRGB(in vec3 rgb)
{
	ivec3 irgb = ivec3(rgb*denominator*(1.+1./denominator*0.5));
	irgb <<= ivec3(20, 10, 0);
	return irgb.x | irgb.y | irgb.z;
}
vec3 unpackRGB(in uint irgb)
{
//	vec3 rgb = vec3((uvec3(irgb) >> uvec3(21, 10, 0)) & ((uvec3(1)<<uvec3(11, 11, 10))-1));
	vec3 rgb = vec3((uvec3(irgb) >> uvec3(20, 10, 0)) & ((uvec3(1)<<uvec3(10, 10, 10))-1));
	return rgb / denominator;
}

#endif // VOXELIZE_GLSL_