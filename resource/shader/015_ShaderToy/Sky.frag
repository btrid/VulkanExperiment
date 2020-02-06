#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_RenderTarget 0
#include "applib/System.glsl"

layout(set=1, binding=0) uniform sampler3D s_map;
layout(set=1, binding=1, r8ui) uniform uimage3D i_map;

layout(location = 1) in Vertex 
{
	vec3 Position;
}FSIn;

layout(push_constant) uniform Input
{
	mat4 PV;
	vec4 CamPos;
} constant;

layout(location=0) out vec4 FragColor;



// 雲の光の吸収量
#define ABSORPTION		0.7

const float u_plant_radius = 1000.;
const vec4 u_planet = vec4(0., -u_plant_radius, 0, u_plant_radius);
const vec4 u_cloud_inner = vec4(u_planet.xyz, u_planet.w*1.05);
const vec4 u_cloud_outer = u_cloud_inner + vec4(0., 0., 0, 30.);
const float u_cloud_area_inv = 1. / (u_cloud_outer.w - u_cloud_inner.w);
vec3 uLightRay = normalize(vec3(0., 1., 0.));
vec3 uLightColor = vec3(1.);
vec3 uIndirectColor = vec3(0.6);


vec3 getSkyColor(in vec3 ray)
{
	// 太陽の色
	vec3 vSkyColor = mix(vec3(.0, 0.1, 0.4), vec3(0.3, 0.6, 0.8), 1.0 - ray.y);
	// 太陽
	vSkyColor = mix(vSkyColor, uLightColor, smoothstep(0.999, 1.00, dot(uLightRay, ray)));

	uIndirectColor = mix(vSkyColor, vec3(0.4), 0.2);
	return vSkyColor;
}


float cloud_density(in vec3 pos)
{
	float t = (distance(pos,u_cloud_inner.xyz)-u_cloud_inner.w)*u_cloud_area_inv;
	pos = vec3(pos.x, t, pos.z) / vec3(2000., 1., 2000.) + vec3(0.5, 0., 0.5);

	float density = texture(s_map, pos).x;

	return density;
}

float cloud_lighting(in vec3 pos)
{
	float transparent = 1.;
	for (int i = 0; i < 1; i++) 
	{
		pos = pos + uLightRay * 4.;
		float density = cloud_density(pos);

		transparent *= exp(-density * ABSORPTION);
	}
	return transparent;
}

vec4 cloud(in vec3 rayPos, in vec3 rayDir)
{
	float transparent = 1.;
	float alpha = 0.;
	vec3 color = vec3(0.);
	rayPos += rayDir * 0.5;
	for(int step = 0; step < 30; step++)
	{
		float density = cloud_density(rayPos);

		float t = exp(-density * ABSORPTION);
		transparent *= t;
		if (transparent < 0.01) { break; }

		color += transparent * density * cloud_lighting(rayPos);
		alpha += (1.-t) *(1.-alpha);

		rayPos += rayDir;
	}
	return vec4(color, min(alpha, 1.));
}

void main()
{
	vec3 sky_color = getSkyColor(normalize(FSIn.Position-constant.CamPos.xyz));

	vec4 color = cloud(FSIn.Position, normalize(FSIn.Position-constant.CamPos.xyz));

	color.xyz = mix(sky_color, color.xyz/(color.a+0.0001), color.a);
	FragColor = vec4(color.xyz, 1.);

}