#ifndef WORLEYNOISE_HEADER_
#define WORLEYNOISE_HEADER_

#ifdef USE_WORLEYNOISE
layout(set=USE_WORLEYNOISE, binding=0) uniform sampler3D s_cloud_map;
layout(set=USE_WORLEYNOISE, binding=1) uniform sampler3D s_cloud_detail_map;
layout(set=USE_WORLEYNOISE, binding=2) uniform sampler2D s_cloud_distort_map;
layout(set=USE_WORLEYNOISE, binding=3) uniform sampler2D s_weather_map;
layout(set=USE_WORLEYNOISE, binding=10, rgba8ui) uniform uimage3D i_cloud_map;
layout(set=USE_WORLEYNOISE, binding=11, rgba8ui) uniform uimage3D i_cloud_detail_map;
layout(set=USE_WORLEYNOISE, binding=12, rg8ui) uniform uimage2D i_cloud_distort_map;
layout(set=USE_WORLEYNOISE, binding=13, rgba8ui) uniform uimage2D i_weather_map;


vec3 _wn_rand(in ivec4 co)
{
	vec3 s = vec3(dot(vec3(co.xyz)*9.63+53.81, vec3(12.98,78.23, 15.61)), dot(vec3(co.zxy)*54.53+37.33, vec3(91.87,47.73, 13.78)), dot(vec3(co.yzx)*18.71+27.14, vec3(51.71,14.35, 24.89)));
	return fract(sin(s) * (float(co.w) + vec3(39.15, 48.51, 67.79)) * vec3(39729.1, 43758.5, 63527.7));
}

float worley_noise(in ivec3 invocation, in int level, in ivec3 reso)
{
	vec4 value = vec4(0.);
	for(int i = 0; i < 4; i++)
	{
		ivec3 tile_size = max(ivec3(512)>>(level+i), ivec3(1));
		ivec3 tile_id = invocation/tile_size;
//		uvec3 tile_id = uvec3(floor(vec3(invocation)/vec3(tile_size));
		vec3 pos = vec3(invocation%tile_size);
		ivec3 reso_ = reso / tile_size;

		float _radius = float(tile_size.x);
		#define cell_size 1

		for(int z = -cell_size; z <= cell_size; z++)
		for(int y = -cell_size; y <= cell_size; y++)
		for(int x = -cell_size; x <= cell_size; x++)
		{
			ivec3 tid = tile_id + ivec3(x, y, z);
			tid = (tid + reso_) % reso_;
			for(int n = 0; n < 4; n++)
			{
				vec3 p = _wn_rand(ivec4(tid, n))*tile_size + vec3(x, y, z)*tile_size;

				float v = 1.-min(distance(pos, p) / _radius, 1.);
				value[i] = max(value[i], v);
			}
		}
	}
	return dot(value, vec4(0.3, 0.25, 0.2, 0.15));
}

float _v_rand(in vec3 co, in vec3 scale)
{
	return fract(sin(dot(mod(co, scale), vec3(12.67, 78.23, 45.41))) * 43758.5);
}
vec3 _interpolate(in vec3 t) 
{
    return t * t * t * (10. + t * (-15. + 6. * t));
}

float _v_noise(in vec3 pos, in vec3 scale)
{

	vec3 ip = floor(pos);
	vec3 fp = _interpolate(fract(pos));
	vec2 offset = vec2(0., 1.);
	vec4 a = vec4(_v_rand(ip+offset.xxx, scale),_v_rand(ip+offset.yxx, scale),_v_rand(ip+offset.xyx, scale),_v_rand(ip+offset.yyx, scale));
	vec4 b = vec4(_v_rand(ip+offset.xxy, scale),_v_rand(ip+offset.yxy, scale),_v_rand(ip+offset.xyy, scale),_v_rand(ip+offset.yyy, scale));
	a = mix(a, b, fp.z);
	a.xy = mix(a.xy, a.zw, fp.y);
	return mix(a.x, a.y, fp.x);
}


float _v_fBM(in vec3 pos, in uvec3 reso)
{
//	reso = uvec3(8);
	float lacunarity = 2.;
	vec4 value = vec4(0.);
	for(int i = 0; i < 4; i++)
	{
		value[i] = _v_noise(pos, vec3(reso));
		pos  *= 2;
		reso *= 2;
	}

	return dot(value, vec4(0.3, 0.25, 0.2, 0.15));
}

float value_noise(in vec3 invocation, in int level, in uvec3 reso)
{
	vec3 pos = vec3(invocation)/float(1<<max(8-level, 0));
	reso >>= max(8-level, 0);
	return _v_fBM(pos, reso);
}
float value_noise(in vec3 invocation, in int level)
{
	return value_noise(invocation, level, uvec3(9999999));
}

float _c_rand(in vec3 co, in uvec3 scale)
{
	return fract(sin(dot(mod(co, vec3(scale)), vec3(12.67, 78.23, 45.41))) * 43758.5);
}
vec3 _c_interpolate(in vec3 t) 
{
    return t * t * t * (10. + t * (-15. + 6. * t));
}

float _c_noise(in vec3 pos, in uvec3 scale)
{

	vec3 ip = floor(pos);
	vec3 fp = _c_interpolate(fract(pos));
	vec2 offset = vec2(0., 1.);
	vec4 a = vec4(_c_rand(ip+offset.xxx, scale),_c_rand(ip+offset.yxx, scale),_c_rand(ip+offset.xyx, scale),_c_rand(ip+offset.yyx, scale));
	vec4 b = vec4(_c_rand(ip+offset.xxy, scale),_c_rand(ip+offset.yxy, scale),_c_rand(ip+offset.xyy, scale),_c_rand(ip+offset.yyy, scale));
	a = mix(a, b, fp.z);
	a.xy = mix(a.xy, a.zw, fp.y);
	return mix(a.x, a.y, fp.x);
}


float _c_fBM_(in vec3 pos, in uvec3 reso)
{
	vec4 value = vec4(0.);
	for(int i = 0; i < 4; i++)
	{
		value[i] = _c_noise(pos, reso);
		pos  *= 2.;
		reso *= uvec3(2);
	}

	return dot(value, vec4(0.45, 0.3, 0.15, 0.1));
}

vec3 _c_fBM(in vec3 pos, in uvec3 reso)
{
	vec3 value = vec3(0.);
	value.x = _c_fBM_(pos.xyz, reso);
	value.y = _c_fBM_(pos.yzx, reso);
	value.z = _c_fBM_(pos.zxy, reso);

	return value;
}
vec3 curl_noise(in vec3 p, in float d, in uvec3 reso)
{	
	vec2 v = vec2(0., d);

	vec3 p_x0 = _c_fBM(p-v.xxy, reso);
	vec3 p_x1 = _c_fBM(p+v.xxy, reso);
	vec3 p_y0 = _c_fBM(p-v.xyx, reso);
	vec3 p_y1 = _c_fBM(p+v.xyx, reso);
	vec3 p_z0 = _c_fBM(p-v.yxx, reso);
	vec3 p_z1 = _c_fBM(p+v.yxx, reso);

	float x = p_y1.z - p_y0.z - p_z1.y + p_z0.y;
	float y = p_z1.x - p_z0.x - p_x1.z + p_x0.z;
	float z = p_x1.y - p_x0.y - p_y1.x + p_y0.x;

	return vec3(x,y,z)/d;
}


#endif // USE_WORLEYNOISE
#endif // WORLEYNOISE_HEADER_