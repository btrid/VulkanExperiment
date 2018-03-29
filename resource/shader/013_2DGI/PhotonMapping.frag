#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Common.glsl>
#include <btrlib/Math.glsl>

#define USE_PM 0
#include <PM.glsl>

layout(location=1) in Data
{
	vec2 texcoord;
}in_data;

layout(location = 0) out vec4 FragColor;
void main()
{
	int hierarchy_offset[8];
	hierarchy_offset[0] = 0;
	hierarchy_offset[1] = hierarchy_offset[0] + u_pm_info.m_resolution.x * u_pm_info.m_resolution.y / (1 * 1);
	hierarchy_offset[2] = hierarchy_offset[1] + u_pm_info.m_resolution.x * u_pm_info.m_resolution.y / (2 * 2);
	hierarchy_offset[3] = hierarchy_offset[2] + u_pm_info.m_resolution.x * u_pm_info.m_resolution.y / (4 * 4);
	hierarchy_offset[4] = hierarchy_offset[3] + u_pm_info.m_resolution.x * u_pm_info.m_resolution.y / (8 * 8);
	hierarchy_offset[5] = hierarchy_offset[4] + u_pm_info.m_resolution.x * u_pm_info.m_resolution.y / (16 * 16);
	hierarchy_offset[6] = hierarchy_offset[5] + u_pm_info.m_resolution.x * u_pm_info.m_resolution.y / (32 * 32);

	const vec2 default_cell_size = vec2(1.);
	const ivec4 reso = ivec4(u_pm_info.m_resolution.xy, u_pm_info.m_resolution.xy/8);

	const ivec2 pixel = ivec2(in_data.texcoord * u_pm_info.m_resolution.xy);
	const vec2 end = vec2(pixel);
	vec3 photon = vec3(0.);

	const ivec2 tile_index_2d = pixel / ivec2(u_pm_info.m_emission_tile_size.xy);
	const int tile_index = int(tile_index_2d.x + tile_index_2d.y*u_pm_info.m_emission_tile_num.x);

	int emission_num = min(b_emission_tile_counter[tile_index.x], 64);
	for(int i = 0; i < emission_num; i++)
	{
		int emission_index = b_emission_tile_map[tile_index*u_pm_info.m_emission_tile_map_max+i];
		Emission emission = b_emission[emission_index];
		vec4 emission_value = emission.value;
		vec4 emission_pos = emission.pos;
		const vec2 start = emission_pos.xz;
		vec2 pos = start;
		ivec2 map_index = ivec2(pos / default_cell_size.xy);


		const vec2 diff = (end - pos);
		if(dot(diff, diff) <= 0.01)
		{
			photon += vec3(0., 0., 1.) * emission_pos.w;
			continue;
		}
		const float ray_dist = length(diff.xy);

		vec2 dir = normalize(diff);
		dir.x = abs(dir.x) <= FLT_EPSIRON ? FLT_EPSIRON*2. : dir.x;
		dir.y = abs(dir.y) <= FLT_EPSIRON ? FLT_EPSIRON*2. : dir.y;

		const ivec3 next_step = ivec3((dir.x < 0. ? -1 : 1), (dir.y < 0. ? -1 : 1), 0);

		for(;;)
		{
			int hierarchy = 0;
			for(; hierarchy<6; hierarchy++)
			{
				ivec2 findex2d = map_index>>hierarchy;
				int findex = findex2d.x + findex2d.y*(u_pm_info.m_resolution.x>>hierarchy);
				if(b_fragment_map[findex + hierarchy_offset[hierarchy]] != 0)
				{
					break;
				}
			}

			// march
			{
				vec2 cell_size = default_cell_size*(1<<hierarchy);
				vec2 cell_origin = vec2(map_index>>hierarchy)*cell_size;
				vec2 cell_p = pos - cell_origin;

				float x = dir.x < 0. ? cell_p.x : (cell_size.x- cell_p.x);
				float y = dir.y < 0. ? cell_p.y : (cell_size.y- cell_p.y);

				x = x <= FLT_EPSIRON ? (x+cell_size.x) : x;
				y = y <= FLT_EPSIRON ? (y+cell_size.y) : y;

				vec2 dist = abs(vec2(x, y) / dir);
				int next_ = dist.x < dist.y ? 0 : 1;
				pos += dir * dist[next_];

				ivec2 next = next_ == 0 ? next_step.xz : next_step.zy;
				map_index = ivec2((pos+ next*0.1) / default_cell_size.xy);
			}

			// 終了判定
			if(distance(start.xy, pos.xy) >= ray_dist.x)
			{
				// 距離を超えたら光がフラグメントにヒット
				photon += vec3(0., 0., 1.) * emission_pos.w / (1+ray_dist.x*ray_dist.x);
				break;
			}
			// hit確認
			{
				ivec2 fragment_index = map_index/8;
				int fragment_index_1d = fragment_index.x + fragment_index.y*reso.z;
				uint64_t fragment_map = b_fragment_hierarchy[fragment_index_1d];

				ivec2 fragment_index_sub = map_index%8;
				int fragment_index_sub_1d = fragment_index_sub.x + fragment_index_sub.y*8;
				uint64_t bit = uint64_t(1)<<fragment_index_sub_1d;

				if((fragment_map & bit) != 0)
				{ 
					break;
				}
			}
		}
	}
//	const int pixel_1d = pixel.x + pixel.y*u_pm_info.m_resolution.x;
//	b_color[pixel_1d] = vec4(photon, 1.);
	FragColor = vec4(photon, 1.);
}
