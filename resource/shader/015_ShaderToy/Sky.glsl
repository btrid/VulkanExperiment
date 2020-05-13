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
layout(set=USE_Sky, binding=10, r16ui) uniform uimage3D i_shadow_map;
layout(set=USE_Sky, binding=11, rgba16) uniform image2D i_render_map;


layout(push_constant) uniform Input
{
	vec3 window;
	vec4 light_front;
	float coverage_min;
	float coverage_max;
	float inscattering_sampling_offset;
	float inscattering_rate;
	int sample_num;
	int _1;
	int _2;
	int _3;
	vec4 test;
} constant;
#define uLightRay constant.light_front.xyz

struct Planet
{
	vec3 m_pos;
	float m_radius;
	vec2 m_cloud_area;
};
Planet u_planet = {vec3(0., -1000., 0.), 1000., vec2(1050., 1082.)};


vec3 uLightColor = vec3(1.);

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

vec4 intersectPlane(vec3 orig,vec3 dir, vec3 planeOrig, vec3 planeNormal)
{
	float d = dot(dir, planeNormal);

	if(abs(d) < 0.00001)
	{
		// 平行なら当たらない
		return vec4(-1.);
	}
	float dist =  dot(planeOrig - orig, planeNormal) / d;
	if (dist <= 0.) 
	{
		// 違う方向
//		return vec4(-1.);
	}
	return vec4(dist);
}


int intersectRayAtomEx(vec3 Pos, vec3 Dir, vec3 AtomPos, vec2 Area, float z, out vec4 Rays)
{
	vec3 RelativePos = AtomPos - Pos;
	float tca = dot(RelativePos, Dir);

	vec2 RadiusSq = Area * Area;
	float d2 = dot(RelativePos, RelativePos) - tca * tca;
	bvec2 intersect = greaterThanEqual(RadiusSq, vec2(d2));
//	vec4 dist = vec4(tca) + vec4(sqrt(RadiusSq.yxxy - d2)) * vec4(-1., -1., 1., 1.);
	vec4 dist = vec4(tca) + vec4(sqrt(abs(RadiusSq.yxxy - d2))) * vec4(-1., -1., 1., 1.);

	int count = 0;
	if (intersect.x && dist.y >= 0.)
	{
		Rays[count*2] = max(dist.x, 0.);
		Rays[count*2+1] = dist.y;
        count++;
	}
	if (intersect.y && dist.w >= 0.)
	{
		float begin = intersect.x ? max(dist.z, 0.) : max(dist.x, 0.);
		if(z < 0. || begin < z)
		{
			Rays[count*2] = intersect.x ? max(dist.z, 0.) : max(dist.x, 0.);
			Rays[count*2+1] = z>=0. ? min(dist.w, z) : dist.w;
			count++;
		}
	}
	return count;
}
int intersectRayAtmosphere(vec3 Pos, vec3 Dir, vec3 AtmospherePos, vec2 Area, float z, out vec4 Rays)
{
	return intersectRayAtomEx(Pos, Dir, AtmospherePos, Area, z, Rays);
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
float remap(float original_value, float original_min, float original_max, float new_min, float new_max){ return (original_value - original_min) / (original_max - original_min) * (new_max - new_min) + new_min; }
float band(in float start, in float peak, in float end, in float t){return smoothstep (start, peak, t) * (1. - smoothstep (peak, end, t));}
float band2(in float ls, in float le, in float hs, in float he, in float t){return smoothstep (ls, le, t) * (1. - smoothstep (hs, he, t));}
float atan2(in float y, in float x){return x == 0.0 ? sign(y)*3.14/2.:atan(y, x);}

//float henyeyGreenstein(float d, float g) { return ((1. - g*g) / pow((1. + g*g - 2.*g*d), 1.5)) / (4.*3.1415); }
float henyeyGreenstein(float d, float g) { return ((1. - g*g) / pow((1. + g*g - 2.*g*d), 1.5)) * 0.5; }
float BeerLambert(float density){ return exp(-density);}
float PowderSugarEffect(float density){ return 1. - exp(-density*2.);}
float LightEnergy(float density, float henyeyGreensteinFactor)
{
	float beer_lambert = BeerLambert(density);
	float powder_sugar_effect = PowderSugarEffect(density);
	return 2. * beer_lambert * powder_sugar_effect * henyeyGreensteinFactor;
}

vec3 sampleWeather(vec3 pos)
{
	vec3 p = pos - u_planet.m_pos;	
	vec2 uv = p.xz / vec2(u_planet.m_cloud_area.y) * vec2(0.5) + vec2(0.5); 

	return texture(s_weather_map, uv + constant.window.xz*0.).xyz; 
}
float getPrecipitation(vec3 weather_data){ return mix(0., 0.3, weather_data.g) + 0.001; }
float getCloudType(vec3 weather_data){ return weather_data.b; }
float heightFraction(vec3 pos) 
{
	return (distance(pos,u_planet.m_pos)-u_planet.m_cloud_area.x)/(u_planet.m_cloud_area.y-u_planet.m_cloud_area.x);
}

vec4 mixGradients(float cloud_type)
{
	#define STRATUS_GRADIENT vec4(0.02, 0.05, 0.09, 0.11)
	#define STRATOCUMULUS_GRADIENT vec4(0.02, 0.2, 0.48, 0.625)
	#define CUMULUS_GRADIENT vec4(0.01, 0.0625, 0.78, 1.0)

	// cloud_type is... 0.0 = stratus, 0.5 = stratocumulus, 1.0 = cumulus
	float stratus = 1.0 - saturate(cloud_type * 2.);
	float stratocumulus = 1.0 - abs(cloud_type - 0.5) * 2.0;
	float cumulus = saturate(cloud_type - 0.5) * 2.0;
	return STRATUS_GRADIENT * stratus + STRATOCUMULUS_GRADIENT * stratocumulus + CUMULUS_GRADIENT * cumulus;
}
float densityHeightGradient(vec3 weather_data, float height_frac)
{
	vec4 cloud_gradient = mixGradients(getCloudType(weather_data));
	return band2(cloud_gradient.x, cloud_gradient.y, cloud_gradient.z, cloud_gradient.w, height_frac);
}

float cloud_density(vec3 pos, vec3 weather_data, float height_frac, float lod, bool use_detail)
{
	if(height_frac>= 1. || height_frac <= 0.) { return 0.; } //範囲外

	float height_gradient = densityHeightGradient(weather_data, height_frac);
	if(height_gradient<=0.){ return 0.;} // この雲タイプはない高さ

//	float cloud_coverage = saturate(weather_data.r*constant.coverage_min);
//	float cloud_coverage = smoothstep(constant.coverage_min, constant.coverage_max, weather_data.r)*weather_data.r;
	float cloud_coverage = weather_data.r;

	vec3 p = pos - u_planet.m_pos;
	vec3 uv = vec3(p.x, height_frac, p.z) / vec3(u_planet.m_cloud_area.y, 1., u_planet.m_cloud_area.y);

	vec4 low_freq_noise = texture(s_cloud_map, uv);
	float low_freq_fBM = dot(low_freq_noise.gba, vec3(0.625, 0.25, 0.125));
	float base_cloud = remap(low_freq_noise.r, -(1.-low_freq_fBM), 1., 0., 1.);

	base_cloud *= height_gradient;

	float final_cloud = remap(base_cloud, cloud_coverage, 1., 0., 1.) * cloud_coverage;

    {
//		vec2 uv = pos.xz*constant.test.y;
//		pos *= vec3(4., 4., 4.);
		vec3 high_freq_noise = texture(s_cloud_detail_map, uv).xyz;
		float high_freq_fBM = dot(high_freq_noise, vec3(0.625, 0.25, 0.125));
		float high_freq_foise_modifier = mix(high_freq_fBM, 1.0-high_freq_fBM, saturate(height_frac * 10.));

		final_cloud = remap(final_cloud, high_freq_foise_modifier*0.2, 1., 0., 1.);
    }
//	final_cloud = smoothstep(0.3, 0.3, final_cloud);
//	final_cloud = smoothstep(constant.coverage_min, constant.coverage_max, final_cloud) * final_cloud;
	return saturate(final_cloud);

}

#endif


#endif