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
	FragColor = b_color[index.x + index.y * int(u_pm_info.m_resolution.x)]*10.;
}