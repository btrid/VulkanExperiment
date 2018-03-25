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
	ivec2 index = ivec2(in_modeldata.texcoord * u_pm_info.m_resolution.xy);
//	uint rgba = b_color[index.x + index.y * int(u_pm_info.m_resolution.x)];
//	FragColor = unpackUnorm4x8(rgba);
	uvec4 rgba = b_color[index.x + index.y * int(u_pm_info.m_resolution.x)];
	FragColor = vec4(0., 0., 0., 1.);
	FragColor.b = rgba.b * 0.01;
}