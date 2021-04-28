
// https://github.com/zerowidth/jps-explained
// https://zerowidth.com/2013/05/05/jump-point-search-explained.html


layout(push_constant, std430) uniform InputVertex
{
	i16vec2 target[10];
	i16vec2 target_num;
	i16vec2 reso;
	int age;
	int dir_type;
} constant;

uvec4 g_neighor_check_list[] =
{
	uvec4(2,6,1,7), // diagonal_path 
	uvec4(3,5,4,4), // diagonal_wall
	uvec4(1,7,4,4), // straight_path
	uvec4(2,6,4,4), // straight_wall
};

void tryPushOpen(ivec2 pos, uint dir_type, uint cost)
{
	pos += g_neighbor[dir_type];
	int index = pos.x + pos.y * constant.reso.x;

	{
		// すでにチェック済みなら終わり
		uint prev_cost = atomicCompSwap(b_path_data[index].data, -1, dir_type|((cost+1)<<4));
		if(prev_cost != -1)
		{
			return;
		}
	}

	int offset = int(((constant.age+1)%2)*2 + (dir_type%2));
	uint active_index = atomicAdd(b_open_counter[offset].w, 1);
	if((active_index%64)==0) atomicAdd(b_open_counter[offset].x, 1);
	b_open[offset*2048+active_index] = i16vec2(pos);
}

void explore(in ivec2 pos, uint dir_type, uint cost)
{
	int index = pos.x + pos.y * constant.reso.x;

	// 新しい探索のチェック
	{
		uint neighbor = uint(b_neighbor[index]);
		uvec4 path_check = (dir_type.xxxx + g_neighor_check_list[(dir_type%2)*2]) % uvec4(8);
		uvec4 wall_check = (dir_type.xxxx + g_neighor_check_list[(dir_type%2)*2+1]) % uvec4(8);
		uvec4 path_bit = uvec4(1)<<path_check;
		uvec4 wall_bit = uvec4(1)<<wall_check;

		bvec4 is_path = notEqual((~neighbor.xxxx) & path_bit, uvec4(0));
		bvec4 is_wall = notEqual(neighbor.xxxx & wall_bit, uvec4(0));
		uvec4 is_open = uvec4(is_path) * uvec4(is_wall.xy, 1-ivec2(dir_type%2));

		uint is_advance = uint(((~neighbor) & (1u << dir_type)) != 0) * uint(any(is_path.zw));

		for(int i = 0; i < 4; i++)
		{
			if(is_open[i]==0){ continue; }
			tryPushOpen(pos, path_check[i], cost);
		}

		if(is_advance != 0)
		{
			tryPushOpen(pos, dir_type, cost);
		}
	}
}
