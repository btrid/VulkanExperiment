

struct MapInfo
{
	vec4 cell_size;
	uvec4 m_cell_num;
};

#ifdef SETPOINT_MAP
layout(set=SETPOINT_MAP, binding=0, r8ui) uniform readonly uimage2D t_map;
layout(std140, set=SETPOINT_MAP, binding=1) uniform MapInfoUniform {
	MapInfo u_map_info;
};


#endif
