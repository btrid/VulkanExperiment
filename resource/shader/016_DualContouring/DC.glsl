#ifndef DC_H_
#define DC_H_

#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#if defined(USE_DC)

struct VoxelInfo
{
	vec4 u_area_min;
	vec4 u_area_max;
	vec4 u_cell_size;
	uvec4 u_cell_num;
};

#if defined(USE_DC)
layout(std140, set=USE_DC, binding = 0) restrict uniform VoxelInfoUniform
{
	VoxelInfo u_voxel_info;
};
layout (set=USE_DC, binding=1, r8ui) restrict uniform uimage3D t_voxel_image[8];
layout (set=USE_DC, binding=2) uniform sampler3D t_voxel_sampler;
#endif

ivec3 getVoxelIndex(in VoxelInfo info, in vec3 pos) 
{
	return ivec3((pos - info.u_area_min.xyz) / info.u_cell_size.xyz);
}

#endif

#endif