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
	vec3 rgb = vec3(0.);
	rgb.rg += texture(s_color[0], vec3(in_modeldata.texcoord, 0.)).rg*1000.;
	rgb.b += texture(s_color[0], vec3(in_modeldata.texcoord, 1.)).r*1000.;
	rgb.rg += texture(s_color[1], vec3(in_modeldata.texcoord, 0.)).rg*30.;
	rgb.b += texture(s_color[1], vec3(in_modeldata.texcoord, 1.)).r*30.;
	FragColor = vec4(rgb, 1.);
}