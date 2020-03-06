#ifndef WORLEYNOISE_HEADER_
#define WORLEYNOISE_HEADER_

#ifdef USE_WORLEYNOISE
layout(set=USE_WORLEYNOISE, binding=0) uniform sampler3D s_cloud_map;
layout(set=USE_WORLEYNOISE, binding=10, rgba8ui) uniform uimage3D i_cloud_map;
layout(set=USE_WORLEYNOISE, binding=1) uniform sampler3D s_cloud_detail_map;
layout(set=USE_WORLEYNOISE, binding=11, rgba8ui) uniform uimage3D i_cloud_detail_map;
layout(set=USE_WORLEYNOISE, binding=2) uniform sampler2D s_weather_map;
layout(set=USE_WORLEYNOISE, binding=12, rgba8ui) uniform uimage2D i_weather_map;


vec3 _wn_rand(in ivec4 co)
{
	vec3 s = vec3(dot(vec3(co.xyz)*9.63+53.81, vec3(12.98,78.23, 15.61)), dot(vec3(co.zxy)*54.53+37.33, vec3(91.87,47.73, 13.78)), dot(vec3(co.yzx)*18.71+27.14, vec3(51.71,14.35, 24.89)));
	return fract(sin(s) * (float(co.w) + vec3(39.15, 48.51, 67.79)) * vec3(39729.1, 43758.5, 63527.7));
}

float worley_noise(in uvec3 invocation, in int level)
{
	vec3 value = vec3(0.);

	for(int i = 0; i < 3; i++)
	{
		uvec3 tile_size = ivec3(128)>>(level+i);
		uvec3 tile_id = invocation/tile_size;
		vec3 pos = vec3(invocation%tile_size);
		float _radius = float(tile_size.x);
		#define cell_size 1

		for(int z = -cell_size; z <= cell_size; z++)
		for(int y = -cell_size; y <= cell_size; y++)
		for(int x = -cell_size; x <= cell_size; x++)
		{
			ivec3 tid = ivec3(tile_id) + ivec3(x, y, z);
			for(int n = 0; n < 2; n++)
			{
				vec3 p = _wn_rand(ivec4(tid, n))*tile_size + vec3(x, y, z)*tile_size;

				float v = 1.-min(distance(pos, p) / _radius, 1.);
				value[i] = max(value[i], v);
			}
		}
	}
	return dot(value, vec3(0.625, 0.25, 0.125));
}

float _v_rand(in vec3 co)
{
	return fract(sin(dot(co, vec3(12.98,78.23, 45.41))) * 43758.5);
}
float _v_noise(in vec3 pos)
{
	vec3 ip = floor(pos);
	vec3 fp = smoothstep(0., 1., fract(pos));
	vec2 offset = vec2(0., 1.);
	vec4 a = vec4(_v_rand(ip+offset.xxx),_v_rand(ip+offset.yxx),_v_rand(ip+offset.xyx),_v_rand(ip+offset.yyx));
	vec4 b = vec4(_v_rand(ip+offset.xxy),_v_rand(ip+offset.yxy),_v_rand(ip+offset.xyy),_v_rand(ip+offset.yyy));
	a = mix(a, b, fp.z);
	a.xy = mix(a.xy, a.zw, fp.y);
	return mix(a.x, a.y, fp.x);
}


float _v_fBM(in vec3 pos, in int octaves)
{
	float lacunarity = 2.971;
	float total = 0.;
	float value = 0.;
	for(int i = 0; i < octaves; i++)
	{
		value = value*2. + _v_noise(pos);
		total = total*2. + 1.;
		pos = pos * lacunarity;
	}

	return value / total;
}

float value_noise(in vec3 invocation, in int level)
{
	vec3 pos = vec3(invocation) * 1./float(1<<max(7-level, 0)) + 24.53;
	return _v_fBM(pos, 3);
}


#endif // USE_WORLEYNOISE
#endif // WORLEYNOISE_HEADER_