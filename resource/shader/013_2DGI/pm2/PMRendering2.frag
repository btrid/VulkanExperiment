#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>

#define USE_PM 0
#include <PM.glsl>

layout(location=1) in ModelData
{
	vec2 texcoord;
}in_modeldata;

layout(location = 0) out vec4 FragColor;
void main()
{
//	uint rgb = texture(s_color[0], in_modeldata.texcoord).r;
//	uint rgb1 = texture(s_color[1], in_modeldata.texcoord).r;
//	uvec3 range = uvec3((1<<11), (1<<11), (1<<10)) - 1;
//	uvec3 rgb_v = (uvec3(rgb/*rgb1*/) >> uvec3(21, 10, 0)) & range;
//	vec4 rgba = vec4(rgb_v*100. / range, 1.);
//	FragColor = rgba;
	vec3 rgb = texture(s_color[0], in_modeldata.texcoord).rgb;
	vec3 rgb1 = texture(s_color[1], in_modeldata.texcoord).rgb;
	FragColor = vec4(rgb*0.001, 1.);
}