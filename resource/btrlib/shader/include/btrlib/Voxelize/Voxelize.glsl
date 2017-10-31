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
//layout (set=USE_VOXEL, binding=1, rgba32f) restrict uniform texture3D t_voxel_teture;
layout (set=USE_VOXEL, binding=2) uniform sampler3D t_voxel_sampler;
#endif

ivec3 getVoxelIndex(in VoxelInfo info, in vec3 pos) 
{
	return ivec3((pos - info.u_area_min.xyz) / info.u_cell_size.xyz);
}

