#ifndef WORLEYNOISE_HEADER_
#define WORLEYNOISE_HEADER_

#ifdef USE_WORLEYNOISE
layout(set=USE_WORLEYNOISE, binding=0) uniform sampler3D s_worleynoise_map;
layout(set=USE_WORLEYNOISE, binding=10, r8ui) uniform uimage3D i_worleynoise_map;

layout(set=USE_WORLEYNOISE, binding=20, std430) restrict buffer WN_SeedBuffer {
	ivec4 b_seed[];
};
layout(set=USE_WORLEYNOISE, binding=21, std430) restrict buffer WN_TileCounter {
	uint b_tile_counter[];
};
layout(set=USE_WORLEYNOISE, binding=22, std430) restrict buffer WN_TileDataBuffer {
	uint b_tile_buffer[];
};

#define TILE_SIZE 32
#define radius  27.

vec2 _wn_rand(in ivec2 co, in int i)
{
	vec2 s = vec2(dot(vec3(co, i)*9.63+53.81, vec3(12.98,78.23, 15.61)), dot(vec3(co.yx, i)*54.53+37.33, vec3(91.87,47.73, 13)));
	return fract(sin(s) * vec2(43758.5, 63527.7));
}

vec4 worley_noise(in uvec2 invocation)
{
	vec4 value = vec4(0.);
	vec2 pos = vec2(invocation) + 0.5;
	for(int i = 0; i < 4; i++)
	{
		uvec2 tile_size = ivec2(32)>>i;
		uvec2 tile_id = invocation/tile_size;
		float _radius = float(tile_size.x);

		for(int y = -2; y <= 1; y++)
		{
			for(int x = -2; x <= 1; x++)
			{
				ivec2 tid = ivec2(tile_id) + ivec2(x, y);
				for(int n = 0; n < 5; n++)
				{
					vec2 p = _wn_rand(tid, n)*tile_size + tid*tile_size;

					float v = 1.-min(distance(pos.xy, p) / _radius, 1.);
					value[i] = max(value[i], v);

				}
			}
		}
	}

	return value;
}
#endif // USE_WORLEYNOISE
#endif // WORLEYNOISE_HEADER_