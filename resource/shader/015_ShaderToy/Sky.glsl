#ifndef SKY_HEADER_
#define SKY_HEADER_


#extension GL_EXT_shader_explicit_arithmetic_types : require

#define USE_RenderTarget 0
#include "applib/System.glsl"

#define USE_WORLEYNOISE 2
#include "WorleyNoise.glsl"

#define USE_Sky 1
#if defined(USE_Sky)
layout(set=USE_Sky, binding=0) uniform sampler3D s_shadow_map;
layout(set=USE_Sky, binding=1) uniform sampler2D s_render_map;
layout(set=USE_Sky, binding=10, r8ui) uniform uimage3D i_shadow_map;
layout(set=USE_Sky, binding=11, rgba16) uniform image2D i_render_map;

// 雲の光の吸収量
#define ABSORPTION		0.15

const float u_plant_radius = 10000.;
const vec4 u_planet = vec4(0., -u_plant_radius, 0, u_plant_radius);
const vec4 u_cloud_inner = vec4(u_planet.xyz, u_planet.w+2000.);
const vec4 u_cloud_outer = u_cloud_inner + vec4(0., 0., 0, 64.);
const float u_cloud_area_inv = 1. / (u_cloud_outer.w - u_cloud_inner.w);
const float u_mapping = 1./u_cloud_outer.w;
vec3 uLightRay = -normalize(vec3(0., 1., 0.));
vec3 uLightColor = vec3(130.);


#define saturate(_a) clamp(_a, 0., 1.)

bool intersectRayAtom(vec3 Pos, vec3 Dir, vec3 AtomPos, vec2 Area, out vec4 OutDist)
{
	vec3 RelativePos = AtomPos - Pos;
	float tca = dot(RelativePos, Dir);

	vec2 RadiusSq = Area * Area;
	float d2 = dot(RelativePos, RelativePos) - tca * tca;

	OutDist = vec4(tca) + vec4(sqrt(RadiusSq.yxxy - d2)) * vec4(-1., -1., 1., 1.);
	return d2 <= RadiusSq.y;
}

float intersectRaySphere(in vec3 origin, in vec3 ray, in vec4 sphere)
{
	vec3 relative_pos = sphere.xyz - origin;
	float tca = dot(relative_pos, ray);
//	if (tca < 0.) return -1.;

	float radiusSq = sphere.w * sphere.w;
	float d2 = dot(relative_pos, relative_pos) - tca * tca;
	if (d2 > radiusSq)
		return -1.;

	float thc = sqrt(radiusSq - d2);
	float t0 = tca - thc;
	float t1 = tca + thc;
	if (t0 < 0.) t0 = t1;
	return t0;
}
float remap(float value, float oldMin, float oldMax, float newMin, float newMax)
{
	 return newMin + (value - oldMin) / (oldMax - oldMin) * (newMax - newMin); 
}

vec3 sampleWeather(vec3 pos){ return texture(s_weather_map, (vec3(pos) * vec3(u_mapping, 1., u_mapping) + vec3(0.5, 0., 0.5)).xz).xyz; }
float getCoverage(vec3 weather_data){ return weather_data.r; }
float getPrecipitation(vec3 weather_data){ return mix(1., 10., weather_data.g); }
float getCloudType(vec3 weather_data){ return weather_data.b; }// 0.0 = stratus, 0.5 = stratocumulus, 1.0 = cumulus
float heightFraction(vec3 pos) { return (distance(pos,u_cloud_inner.xyz)-u_cloud_inner.w)*u_cloud_area_inv; }

vec4 mixGradients(float cloudType)
{
	#define STRATUS_GRADIENT vec4(0.02, 0.05, 0.09, 0.11)
	#define STRATOCUMULUS_GRADIENT vec4(0.02, 0.2, 0.48, 0.625)
	#define CUMULUS_GRADIENT vec4(0.01, 0.0625, 0.78, 1.0)

	float stratus = 1.0 - saturate(cloudType * 2.);
	float stratocumulus = 1.0 - abs(cloudType - 0.5) * 2.0;
	float cumulus = saturate(cloudType - 0.5) * 2.0;
	return STRATUS_GRADIENT * stratus + STRATOCUMULUS_GRADIENT * stratocumulus + CUMULUS_GRADIENT * cumulus;
}
float densityHeightGradient(float height_frac, float cloudType)
{
	vec4 cloudGradient = mixGradients(cloudType);
	return smoothstep(cloudGradient.x, cloudGradient.y, height_frac) - smoothstep(cloudGradient.z, cloudGradient.w, height_frac);
}

float sampleCloudDensity(vec3 pos, vec3 weather_data, float height_frac, float lod)
{
	if(height_frac>= 1. || height_frac <= 0.) { return 0.; } //範囲外

	pos = vec3(pos.x, height_frac, pos.z) * vec3(u_mapping, 1., u_mapping) + vec3(0.5, 0., 0.5);// UV[0~1]
	
	vec4 low_freq_noise = texture(s_cloud_map, pos);
	float low_freq_fBM = dot(low_freq_noise.yzw, vec3(0.625, 0.25, 0.125));
	float base_cloud = remap(low_freq_noise.r, -(1.0 - low_freq_fBM), 1.0, 0.0, 1.0);

	float density_gradient = densityHeightGradient(height_frac, getCloudType(weather_data));
	base_cloud *= density_gradient;

	float cloud_coverage = getCoverage(weather_data);
	float base_cloud_with_coverage = remap(base_cloud, 1.0 - cloud_coverage, 1., 0., 1.);
	base_cloud_with_coverage *= cloud_coverage;

	//// TODO add curl noise
	//// pos += curlNoise.xy * (1.0f - height_frac);

	vec3 high_freq_noise = texture(s_cloud_detail_map, pos * 0.1).xyz;
	float high_freq_fBM = dot(high_freq_noise, vec3(0.625, 0.25, 0.125));
	float high_freq_foise_modifier = mix(high_freq_fBM, 1.0 - high_freq_fBM, saturate(height_frac * 10.));

	float final_cloud = remap(base_cloud_with_coverage, high_freq_foise_modifier * 0.2, 1.0, 0.0, 1.0);
	return saturate(final_cloud);
}

#endif


#endif