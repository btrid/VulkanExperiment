struct VoxelInfo
{
	vec4 u_area_min;
	vec4 u_area_max;
	vec4 u_cell_size;
	uvec4 u_cell_num;
};

#if defined(SETPOINT_VOXEL)
layout(std140, set=SETPOINT_VOXEL, binding = 0) uniform VoxelInfoUniform
{
	VoxelInfo u_voxel_info;
};
layout (set=SETPOINT_VOXEL, binding = 1) uniform sampler3D t_voxel;


ivec3 getVoxelIndex(/*in VoxelInfo info,*/ in vec3 pos) 
{
	return ivec3((pos - u_voxel_info.u_area_min.xyz) / u_voxel_info.u_cell_size.xyz);
}
#endif