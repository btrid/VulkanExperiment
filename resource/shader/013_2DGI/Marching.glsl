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
