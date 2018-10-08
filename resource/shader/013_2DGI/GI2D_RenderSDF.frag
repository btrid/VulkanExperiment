#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_GI2D_RENDER 1
#include "GI2D.glsl"

layout(location = 0) out vec4 o_FragColor;
void main()
{
	const ivec4 reso = ivec4(u_gi2d_info.m_resolution.xyxy);
	ivec2 coord = ivec2(gl_FragCoord.xy);
	o_FragColor.xyz = b_sdf[coord.x + coord.y*reso.x].xxx / 10.;
//	o_FragColor.xyz = b_jfa[coord.x + coord.y*reso.x].distance.xxx / 512;
	o_FragColor.w = 1.;


}
