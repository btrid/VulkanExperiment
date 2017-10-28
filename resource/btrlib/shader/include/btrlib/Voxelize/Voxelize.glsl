struct VoxelInfo
{
	vec4 u_area_min;
	vec4 u_area_max;
	vec4 u_cell_size;
	uvec4 u_cell_num;
};

#if defined(USE_VOXEL_WRITE)
layout(std140, set=USE_VOXEL_WRITE, binding = 0) restrict uniform VoxelInfoUniform
{
	VoxelInfo u_voxel_info;
};
layout (set=USE_VOXEL_WRITE, binding=1, rgba32f) restrict uniform image3D t_voxel_image[8];
#endif

#if defined(USE_VOXEL_READ)
layout(std140, set=USE_VOXEL_READ, binding = 0) restrict uniform VoxelInfoUniform
{
	VoxelInfo u_voxel_info;
};
layout (set=USE_VOXEL_READ, binding=1) uniform sampler3D t_voxel_albedo;
#endif

ivec3 getVoxelIndex(/*in VoxelInfo info,*/ in vec3 pos) 
{
	return ivec3((pos - u_voxel_info.u_area_min.xyz) / u_voxel_info.u_cell_size.xyz);
}

vec3 unpack(uint packed)
{
	uint count = (packed)&((1<<5)-1);
	if(count != 0)
	{
		uint packed_r = (packed>>23)&((1<<9)-1);
		uint packed_g = (packed>>14)&((1<<9)-1);
		uint packed_b = (packed>>5)&((1<<9)-1);
		vec3 unpacked = vec3(packed_r, packed_g, packed_b) / 64. / float(count);
		return unpacked;
	}
	return vec3(0.);

}

