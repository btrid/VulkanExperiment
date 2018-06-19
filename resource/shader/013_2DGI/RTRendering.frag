#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_PM 0
#define USE_RT 1
#include "PM.glsl"

layout(location=1) in In
{
	vec2 texcoord;
}fs_in;

layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 rgb = texture(s_color, fs_in.texcoord).rgb;
	FragColor = vec4(rgb, 1.);
//	uvec2 rt_index = uvec2(fs_in.texcoord);
//	b_rt_data[rt_index.x + rt_index.y*rt_reso.x];
//	FragColor = vec4(rgb, 1.);

}