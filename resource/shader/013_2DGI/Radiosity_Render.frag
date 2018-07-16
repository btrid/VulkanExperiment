#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"



#define Block_Size (1)

layout(location = 0) out vec4 FragColor;
void main()
{
#if Block_Size == 1
	uint radiance_size = u_gi2d_info.m_resolution.x*u_gi2d_info.m_resolution.y;
	uint index = getMemoryOrder(ivec2(gl_FragCoord.xy));
	vec3 rgb = unpackEmissive(b_radiance[index]) + unpackEmissive(b_radiance[index+radiance_size]);
	FragColor = vec4(rgb, 1.);
#else
	float radiance = 0.;
	for(int y = 0; y < Block_Size; y++){
	for(int x = 0; x < Block_Size; x++){
		ivec2 coord = ivec2(gl_FragCoord.xy);
		coord += + ivec2(x-(Block_Size>>1), y-(Block_Size>>1));
		coord = clamp(coord, ivec2(0), u_gi2d_info.m_resolution);
		uint index = getMemoryOrder(coord);
		radiance += b_radiance[index];
	}}
	FragColor = vec4(radiance.xxx/ 100. / 40, 1.);
#endif
}