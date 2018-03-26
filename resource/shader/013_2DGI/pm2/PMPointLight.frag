#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>

#define USE_PM 0
#include <PM.glsl>

layout(push_constant) uniform InputVertex
{
	vec2 pos;
} constant;

//layout(location = 0) out vec4 FragColor;
void main()
{

	float dist = distance(gl_FragCoord.xy, constant.pos.xy);
	float power = 1./(1.+dist);
	if(power >= 0.1)
	{
		vec2 pos = vec2(gl_FragCoord.xy * u_pm_info.m_resolution.xy);

		int index = atomicAdd(b_emission_counter[0].x, 1);
		b_emission[index].pos = vec4(gl_FragCoord.xyyy);
		b_emission[index].value = vec4(0., 0., 1., 0.);

	}
}