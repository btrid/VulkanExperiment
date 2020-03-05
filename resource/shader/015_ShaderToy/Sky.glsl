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

const float u_plant_radius = 1000.;
const vec4 u_planet = vec4(0., -u_plant_radius, 0, u_plant_radius);
const vec4 u_cloud_inner = vec4(u_planet.xyz, u_planet.w*1.05);
const vec4 u_cloud_outer = u_cloud_inner + vec4(0., 0., 0, 16.);
const float u_cloud_area_inv = 1. / (u_cloud_outer.w - u_cloud_inner.w);
const float u_mapping = 1./u_cloud_outer.w;
vec3 uLightRay = normalize(vec3(0., -1., 0.));
vec3 uLightColor = vec3(1.);

vec3 uCamPos = vec3(0., 1., 0.);
vec3 uCamDir = normalize(vec3(1.5, 1.7, 1.5));
#endif


#endif