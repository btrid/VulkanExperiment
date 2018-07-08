#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"



layout(location = 0) out vec4 FragColor;
void main()
{
	// なんかずれる
	vec2 coord = gl_FragCoord.xy;
	uint index = uint(coord.x+256 + coord.y * u_gi2d_info.m_resolution.x);
	vec3 rgb = vec3(b_radiance_map[index] / 100.);
	FragColor = vec4(rgb, 1.);

}