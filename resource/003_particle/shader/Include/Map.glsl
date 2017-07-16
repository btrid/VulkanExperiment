

struct MapInfo
{
	vec2 cell_size;
	uvec2 m_cell_num;
};

#ifdef SETPOINT_MAP
layout(set=SETPOINT_MAP, binding=0, r8ui) uniform readonly uimage2D t_map;
layout(std140, set=SETPOINT_MAP, binding=1) uniform MapInfoUniform {
	MapInfo u_map_info;
};

void march(inout vec2 pos, inout ivec2 map_index, in vec2 _dir)
{
	float progress = length(_dir);
	if(progress < 0.00000001){ return;}
	vec2 dir = normalize(_dir);

	MapInfo map_info = u_map_info;
	float particle_size = 0.;
	for(;;)
	{
		vec2 cell_origin = vec2(map_index)*map_info.cell_size.xy;
		vec2 cell_p = pos - cell_origin;
		float x = dir.x < 0. ? cell_p.x : (map_info.cell_size.x- cell_p.x);
		float y = dir.y < 0. ? cell_p.y : (map_info.cell_size.y- cell_p.y);
		x = (x <= particle_size ? map_info.cell_size.x + x : x) - particle_size;
		y = (y <= particle_size ? map_info.cell_size.y + y : y) - particle_size;

		vec2 dist = vec2(9999.);
		dist.x = abs(dir.x) < FLT_EPSIRON ? 9999.9 : abs(x / dir.x);
		dist.y = abs(dir.y) < FLT_EPSIRON ? 9999.9 : abs(y / dir.y);
		float rate = min(dist.x, dist.y);
		rate = abs(dir.x) < FLT_EPSIRON ? dist.y : rate;
		rate = abs(dir.y) < FLT_EPSIRON ? dist.x : rate;

		vec2 prog = dir * rate;
		float progLength = length(prog);
		if(progress < progLength)
		{
			// 移動完了
			pos += dir * progress;
			pos = clamp(pos, (vec2(map_index) * map_info.cell_size)+particle_size+FLT_EPSIRON, (map_index+ivec2(1)) * map_info.cell_size-particle_size-FLT_EPSIRON);
			return;
		}
		progress -= progLength;

		ivec2 next = ivec2(0);
		if(dist.x < dist.y){
			next.x = dir.x < 0. ? -1 : 1;
		}
		else
		{
			next.y = dir.y < 0. ? -1 : 1;
		}

		vec2 next_pos = pos + prog + vec2(next)*FLT_EPSIRON;
		uint map = imageLoad(t_map, (map_index + next) ).x;
		if(map == 1) 
		{
#if 1
			// 壁にぶつかったので押し戻す
			next_pos = pos + prog - vec2(next)*FLT_EPSIRON*1000.;

#else
			// 壁にぶつかったので反射
			next_pos = pos.xz + prog - vec2(next) *FLT_EPSIRON;

			vec3 wall = vec2(next);
			p.m_vel.xyz = reflect(p.m_vel.xyz, wall);
#endif
		}else{
			map_index += next;
		}
		pos = next_pos;
		pos = clamp(pos, (vec2(map_index) * map_info.cell_size)+particle_size+FLT_EPSIRON, (map_index+ivec2(1)) * map_info.cell_size-particle_size-FLT_EPSIRON);
	}

}

uint calcMapIndex1D(in uvec2 map_index, in uint db_index)
{
	uint offset = db_index* (u_map_info.m_cell_num.x*u_map_info.m_cell_num.y);
	return offset + convert2DTo1D(map_index, u_map_info.m_cell_num.xy);
}
#endif
