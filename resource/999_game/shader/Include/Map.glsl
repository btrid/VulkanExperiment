

struct MapDescriptor
{
	vec2 m_cell_size;
	uvec2 m_cell_num;
};

struct MapInfo
{
	MapDescriptor m_descriptor[2];
	uvec2 m_subcell;
	float m_timeloop;
};

#define WALL_HEIGHT (5.)

struct SceneData
{
	float m_deltatime;
	float m_totaltime;
};
#ifdef SETPOINT_SCENE
layout(std140, set=SETPOINT_SCENE, binding=0) uniform SceneDataUniform {
	SceneData u_scene_data;
};
float calcEmission(in vec3 pos)
{
	float power = 0.2;
	float start_time_offset = dot(pos, pos) / length(pos);
//	start_time_offset += rand3(pos)*20;
	float time = u_scene_data.m_totaltime*50. - start_time_offset;
	if(time <= 0.){ return power;}
	time = mod(time/10, 20.);
	time = min(time, 1.);
	power += 1-pow(time, 0.8);
	return min(power, 1.);	
}
#endif
#ifdef SETPOINT_MAP
layout(std140, set=SETPOINT_MAP, binding=0) uniform MapInfoUniform {
	MapInfo u_map_info;
};
layout(set=SETPOINT_MAP, binding=1, r8ui) uniform /*readonly*/ uimage2D t_map;
layout(set=SETPOINT_MAP, binding=2, r8ui) uniform readonly uimage2D t_mapsub;
layout(set=SETPOINT_MAP, binding=3, r32ui) uniform uimage2D t_map_damage;

ivec3 calcMapIndex(in MapDescriptor desc, in vec3 p)
{
	vec3 cell_size = vec3(desc.m_cell_size.x, WALL_HEIGHT, desc.m_cell_size.y);
	ivec3 map_index = ivec3(p.xyz / cell_size);
	return map_index;
}

void march(inout vec2 pos, inout ivec2 map_index, in vec2 _dir)
{
	float progress = length(_dir);
	if(progress < 0.00000001){ return;}
	vec2 dir = normalize(_dir);

	MapDescriptor desc = u_map_info.m_descriptor[0];
	float particle_size = 0.;
	for(;;)
	{
		vec2 cell_origin = vec2(map_index)*desc.m_cell_size.xy;
		vec2 cell_p = pos - cell_origin;
		float x = dir.x < 0. ? cell_p.x : (desc.m_cell_size.x- cell_p.x);
		float y = dir.y < 0. ? cell_p.y : (desc.m_cell_size.y- cell_p.y);
		x = (x <= particle_size ? desc.m_cell_size.x + x : x) - particle_size;
		y = (y <= particle_size ? desc.m_cell_size.y + y : y) - particle_size;

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
			pos = clamp(pos, (vec2(map_index) * desc.m_cell_size)+particle_size+FLT_EPSIRON, (map_index+ivec2(1)) * desc.m_cell_size-particle_size-FLT_EPSIRON);
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
		if(map != 0) 
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
		pos = clamp(pos, (vec2(map_index) * desc.m_cell_size)+particle_size+FLT_EPSIRON, (map_index+ivec2(1)) * desc.m_cell_size-particle_size-FLT_EPSIRON);
	}

}

struct MarchResult
{
	vec2 next_pos;
	ivec2 next_map_index;
	float progress;
	bool is_end;
};
MarchResult marchEx(in vec2 pos, in ivec2 map_index, in float progress, in vec2 dir)
{
	MapDescriptor desc = u_map_info.m_descriptor[0];
	float particle_size = 0.;
	vec2 cell_origin = vec2(map_index)*desc.m_cell_size.xy;
	vec2 cell_p = pos - cell_origin;
	float x = dir.x < 0. ? cell_p.x : (desc.m_cell_size.x- cell_p.x);
	float y = dir.y < 0. ? cell_p.y : (desc.m_cell_size.y- cell_p.y);
	x = (x <= particle_size ? desc.m_cell_size.x + x : x) - particle_size;
	y = (y <= particle_size ? desc.m_cell_size.y + y : y) - particle_size;

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
		MarchResult result;
		result.next_pos = pos;
		result.next_map_index = map_index;
		result.progress = 0.;
		result.is_end = true;
		return result;
	}

	ivec2 next = ivec2(0);
	if(dist.x < dist.y){
		next.x = dir.x < 0. ? -1 : 1;
	}
	else
	{
		next.y = dir.y < 0. ? -1 : 1;
	}

	pos += prog;
	progress -= progLength;

	MarchResult result;
	result.next_pos = pos;
	result.next_map_index = map_index + next;
	result.progress = progress;
	result.is_end = false;
	return result;
}

uint calcMapIndex1D(in uvec2 map_index)
{
	MapDescriptor desc = u_map_info.m_descriptor[0];
	return map_index.y * desc.m_cell_num.x + map_index.x;
}

ivec3 marchEx3D(inout vec3 pos, inout ivec3 map_index, in vec3 dir)
{
//	MapInfo map_info = u_map_info;
	MapDescriptor desc = u_map_info.m_descriptor[0];
	vec3 cell_size = vec3(desc.m_cell_size.x, WALL_HEIGHT, desc.m_cell_size.y);
	float particle_size = 0.;
	vec3 cell_origin = vec3(map_index)*cell_size;
	vec3 cell_p = pos - cell_origin;
	float x = dir.x < 0. ? cell_p.x : (cell_size.x- cell_p.x);
	float y = dir.y < 0. ? cell_p.y : (cell_size.y- cell_p.y);
	float z = dir.z < 0. ? cell_p.z : (cell_size.z- cell_p.z);
	x = (x <= particle_size ? cell_size.x + x : x) - particle_size;
	y = (y <= particle_size ? cell_size.y + y : y) - particle_size;
	z = (z <= particle_size ? cell_size.z + z : z) - particle_size;

#if 0
	vec3 dist = vec3(999999.);
	dist.x = abs(dir.x) < FLT_EPSIRON ? 999999.9 : abs(x / dir.x);
	dist.y = abs(dir.y) < FLT_EPSIRON ? 999999.9 : abs(y / dir.y);
	dist.z = abs(dir.z) < FLT_EPSIRON ? 999999.9 : abs(z / dir.z);
#else
	vec3 dist = vec3(x, y, z) / dir;
	dist.x = isinf(dist.x) ? 9999999. : abs(dist.x);
	dist.y = isinf(dist.y) ? 9999999. : abs(dist.y);
	dist.z = isinf(dist.z) ? 9999999. : abs(dist.z);
#endif
	float rate = min(dist.x, min(dist.y, dist.z));

	ivec3 next = ivec3(0);
	if(dist.x < dist.y && dist.x < dist.z){
		next.x = dir.x < 0. ? -1 : 1;
	}
	else if(dist.y < dist.z)
	{
		next.y = dir.y < 0. ? -1 : 1;
	}
	else
	{
		next.z = dir.z < 0. ? -1 : 1;
	}
	pos += dir * rate;
	map_index += next;
	return next;
}
#endif
