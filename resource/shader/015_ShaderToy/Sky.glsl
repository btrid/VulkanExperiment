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
layout(set=USE_Sky, binding=10, rg8ui) uniform uimage3D i_shadow_map;
layout(set=USE_Sky, binding=11, rgba16) uniform image2D i_render_map;

const float u_plant_radius = 6300.;
const vec4 u_planet = vec4(0., -u_plant_radius, 0, u_plant_radius);
const vec4 u_cloud_inner = vec4(u_planet.xyz, u_planet.w*1.025);
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

int intersectRayAtomEx(vec3 Pos, vec3 Dir, vec3 AtomPos, vec2 Area, out vec4 Rays)
{
	vec3 RelativePos = AtomPos - Pos;
	float tca = dot(RelativePos, Dir);

	vec2 RadiusSq = Area * Area;
	float d2 = dot(RelativePos, RelativePos) - tca * tca;
	bvec2 intersect = greaterThanEqual(RadiusSq, vec2(d2));
	vec4 dist = vec4(tca) + vec4(sqrt(RadiusSq.yxxy - d2)) * vec4(-1., -1., 1., 1.);

	int count = 0;
	if (intersect.x && dist.y >= 0.)
	{
		Rays[count*2] = max(dist.x, 0.);
		Rays[count*2+1] = dist.y;
        count++;
	}
	if (intersect.y && dist.w >= 0.)
	{
		Rays[count*2] = intersect.x ? max(dist.z, 0.) : max(dist.x, 0.);
		Rays[count*2+1] = dist.w;
        count++;
	}
	return count;

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
float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	 return new_min + (original_value - original_min) / (original_max - original_min) * (new_max - new_min); 
}
float _band(in float ls, in float le, in float hs, in float he, in float t)
{
	return smoothstep (ls, le, t) * (1. - smoothstep (hs, he, t));
}

vec3 sampleWeather(vec3 pos){ return texture(s_weather_map, (vec3(pos) * vec3(u_mapping, 1., u_mapping) + vec3(0.5, 0., 0.5)).xz).xyz; }
float getCoverage(vec3 weather_data){ return weather_data.r; }
float getPrecipitation(vec3 weather_data){ return mix(0., 1., weather_data.g) + 0.0001; }
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
float densityHeightGradient(vec3 weather_data, float height_frac)
{
	vec4 cloudGradient = mixGradients(getCloudType(weather_data));
	return _band(cloudGradient.x, cloudGradient.y, cloudGradient.z, cloudGradient.w, height_frac);
}

float sampleCloudDensity(vec3 pos, vec3 weather_data, float height_frac, float lod)
{
	if(height_frac>= 1. || height_frac <= 0.) { return 0.; } //範囲外

//	vec3 p = vec3(pos.x, height_frac, pos.z) * vec3(0.217, 1., 0.217);
//	pos = vec3(pos.x, height_frac, pos.z) * vec3(0.8, 1., 0.8);
	pos = vec3(pos.x, height_frac, pos.z) * vec3(u_mapping, 1., u_mapping) * 0.5;
	
	vec4 low_freq_noise = texture(s_cloud_map, pos);
	float low_freq_fBM = dot(low_freq_noise.yzw, vec3(0.625, 0.25, 0.125));
	float base_cloud = remap(low_freq_noise.r, -(1.-low_freq_fBM), 1., 0., 1.);

	base_cloud *= densityHeightGradient(weather_data, height_frac);;

	float cloud_coverage = getCoverage(weather_data);
	float base_cloud_with_coverage = remap(base_cloud, 1.-cloud_coverage, 1., 0., 1.);
	float final_cloud = base_cloud_with_coverage * cloud_coverage;

	if(final_cloud > 0.)
    {
        //// TODO add curl noise
        //// pos += curlNoise.xy * (1.0f - height_frac);

        vec3 high_freq_noise = texture(s_cloud_detail_map, pos * vec3(0.01, 0.1, 0.01)).xyz;
        float high_freq_fBM = dot(high_freq_noise, vec3(0.625, 0.25, 0.125));
        float high_freq_foise_modifier = mix(high_freq_fBM, 1.0-high_freq_fBM, saturate(height_frac * 10.));

        final_cloud = remap(final_cloud, high_freq_foise_modifier*0.2, 1., 0., 1.);
    }
	return saturate(final_cloud);
}

#endif


#endif