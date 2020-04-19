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
layout(set=USE_Sky, binding=1) uniform sampler3D s_along_density_map;
layout(set=USE_Sky, binding=2) uniform sampler2D s_render_map;
layout(set=USE_Sky, binding=10, r16ui) uniform uimage3D i_shadow_map;
layout(set=USE_Sky, binding=11, r16ui) uniform uimage3D i_along_density_map;
layout(set=USE_Sky, binding=12, rgba16) uniform image2D i_render_map;


layout(push_constant) uniform Input
{
	vec3 window;
	vec4 light_front;
} constant;


const float u_planet_radius = 1000.;
const float u_planet_cloud_begin = 50.;
const float u_planet_cloud_end = 50.+32.;
const vec4 u_planet = vec4(0., -u_planet_radius, 0, u_planet_radius);
const vec4 u_cloud_inner = u_planet + vec4(0.,0.,0.,u_planet_cloud_begin);
const vec4 u_cloud_outer = u_planet + vec4(0.,0.,0.,u_planet_cloud_end);
const float u_cloud_area_inv = 1. / (u_planet_cloud_end - u_planet_cloud_begin);
const float u_mapping = 1./u_cloud_outer.w;
//vec3 uLightRay = -normalize(vec3(0., 1., 1.));
#define uLightRay constant.light_front.xyz

vec3 uLightColor = vec3(1.);
float uAmbientPower = 0.2;

#define SkyType_Sphere
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
#if defined(SkyType_Sphere)
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
#else
	Rays[0] = intersectPlane(Pos, Dir, vec3(0., u_planet_cloud_begin, 0.), vec3(0.,-1.,0.)).x;
	Rays[1] = intersectPlane(Pos, Dir, vec3(0., u_planet_cloud_end, 0.), vec3(0.,-1.,0.)).x;
	return Rays[0] > 0. ? 1 : 0;
#endif
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
	 return (original_value - original_min) / (original_max - original_min) * (new_max - new_min) + new_min; 
}
float band(in float start, in float peak, in float end, in float t){return smoothstep (start, peak, t) * (1. - smoothstep (peak, end, t));}
float band2(in float ls, in float le, in float hs, in float he, in float t){return smoothstep (ls, le, t) * (1. - smoothstep (hs, he, t));}
float _band(in float ls, in float le, in float hs, in float he, in float t){return band2(ls, le, hs, he, t);}
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
	 return texture(s_weather_map, ((pos + constant.window*0.)* vec3(u_mapping, 1., u_mapping) * vec3(0.5, 1., 0.5) + vec3(0.5, 0., 0.5)).xz).xyz; 
}
float getCoverage(vec3 weather_data){ return saturate(weather_data.r); }
float getPrecipitation(vec3 weather_data){ return mix(0., 0.3, weather_data.g) + 0.001; }
float getCloudType(vec3 weather_data){ return weather_data.b; }
float heightFraction(vec3 pos) 
{
#if defined(SkyType_Sphere)
	return (distance(pos,u_cloud_inner.xyz)-u_cloud_inner.w)*u_cloud_area_inv; 
#else
	return (pos.y-u_planet_cloud_begin) / (u_planet_cloud_end-u_planet_cloud_begin); 
#endif
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
	if(height_gradient<=0.){ return 0.;}

	float cloud_coverage = getCoverage(weather_data);
	float coverage = 0.35;
	float coverage_gradient = 0.2;
	cloud_coverage *= smoothstep(coverage, coverage+coverage_gradient, cloud_coverage);
	if(cloud_coverage <= 0.) { return 0.; }

	pos = vec3(pos.x, height_frac, pos.z) * vec3(u_mapping, 1., u_mapping);
	
	vec4 low_freq_noise = texture(s_cloud_map, pos);
	float low_freq_fBM = dot(low_freq_noise.gba, vec3(0.625, 0.25, 0.125));
	float base_cloud = remap(low_freq_noise.r, -(1.-low_freq_fBM), 1., 0., 1.);

	base_cloud *= height_gradient;

	float final_cloud = remap(base_cloud, cloud_coverage, 1., 0., 1.) * cloud_coverage;

//	if(final_cloud <= 0.){ return 0.; }
//	if(mod(constant.window.x*0.03, 1.) > 0.5)
    {
//		pos.xz += texture(s_cloud_distort_map, pos.xz).xy * (1. - height_frac);

		vec3 high_freq_noise = texture(s_cloud_detail_map, pos*0.1).xyz;
		float high_freq_fBM = dot(high_freq_noise, vec3(0.625, 0.25, 0.125));
		float high_freq_foise_modifier = mix(high_freq_fBM, 1.0-high_freq_fBM, saturate(height_frac * 10.));

		final_cloud = remap(final_cloud, high_freq_foise_modifier*0.2, 1., 0., 1.);
    }
	return saturate(final_cloud);

}

#endif


#endif