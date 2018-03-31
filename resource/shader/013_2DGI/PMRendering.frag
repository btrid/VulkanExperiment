#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>

#define USE_PM 0
#include <PM.glsl>

layout(location=1) in Data
{
	vec2 texcoord;
}in_data;

layout(location = 0) out vec4 FragColor;
void main()
{
//	ivec2 index = ivec2(in_modeldata.texcoord * u_pm_info.m_resolution.xy);
//	FragColor = b_color[index.x + index.y * int(u_pm_info.m_resolution.x)]*10.;
	vec3 rgb = vec3(0.);
	rgb.rg += texture(s_color[0], vec3(in_data.texcoord, 0.)).rg;
	rgb.b += texture(s_color[0], vec3(in_data.texcoord, 1.)).r;
	rgb.rg += texture(s_color[1], vec3(in_data.texcoord, 0.)).rg;
	rgb.b += texture(s_color[1], vec3(in_data.texcoord, 1.)).r;
	rgb.rg += texture(s_color[2], vec3(in_data.texcoord, 0.)).rg*1000;
	rgb.b += texture(s_color[2], vec3(in_data.texcoord, 1.)).r*1000;
	FragColor = vec4(rgb, 1.);

}