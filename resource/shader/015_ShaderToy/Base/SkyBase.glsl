#define USE_RenderTarget 0
#include "applib/System.glsl"

layout(set=1, binding=0) uniform sampler3D s_map;
layout(set=1, binding=1, r8ui) uniform uimage3D i_map;

// 雲の光の吸収量
#define ABSORPTION		0.7

const float u_plant_radius = 1000.;
const vec4 u_planet = vec4(0., -u_plant_radius, 0, u_plant_radius);
const vec4 u_cloud_inner = vec4(u_planet.xyz, u_planet.w*1.05);
const vec4 u_cloud_outer = u_cloud_inner + vec4(0., 0., 0, 50.);
const float u_cloud_area_inv = 1. / (u_cloud_outer.w - u_cloud_inner.w);
const float u_mapping = 1./u_cloud_outer.w;
vec3 uLightRay = normalize(vec3(0., 1., 0.));
vec3 uLightColor = vec3(1.);

