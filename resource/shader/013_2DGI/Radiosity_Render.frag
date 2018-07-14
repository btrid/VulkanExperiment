#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"



#define Block_Size (9)

layout(location = 0) out vec4 FragColor;
void main()
{
#if 1
	uint index = getMemoryOrder(ivec2(gl_FragCoord.xy));
	float radiance = b_radiance_map[index] / 100.;
	vec3 rgb = vec3(radiance);
	FragColor = vec4(rgb, 1.);
#else
	float radiance = 0.;
	for(int y = 0; y < Block_Size; y++){
	for(int x = 0; x < Block_Size; x++){
		ivec2 coord = ivec2(gl_FragCoord.xy);
		coord += + ivec2(x-(Block_Size>>1), y-(Block_Size>>1));
		coord = clamp(coord, ivec2(0), u_gi2d_info.m_resolution);
		uint index = uint(coord.x + coord.y * u_gi2d_info.m_resolution.x);
		radiance += b_radiance_map[index];
	}}
	FragColor = vec4(radiance.xxx/ 100. / 40, 1.);
#endif
}