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
#if 0
	ivec2 coord = ivec2(gl_FragCoord.xy);
	uint index = uint(coord.x + coord.y * u_gi2d_info.m_resolution.x);
//	vec3 rgb = vec3(b_radiance_map[index]);
	vec3 rgb = vec3(b_radiance_map[index] / 100.);
	FragColor = vec4(rgb, 1.);
#else
	float radiance = 0.;
	for(int y = 0; y < 5; y++){
	for(int x = 0; x < 5; x++){
		ivec2 coord = ivec2(gl_FragCoord.xy);
		coord += + ivec2(x-2, y-2);
		coord = clamp(coord, ivec2(0), u_gi2d_info.m_resolution);
		uint index = uint(coord.x + coord.y * u_gi2d_info.m_resolution.x);
		radiance += b_radiance_map[index]/ 100.;
	}}
	FragColor = vec4(radiance.xxx / 40, 1.);
#endif
}