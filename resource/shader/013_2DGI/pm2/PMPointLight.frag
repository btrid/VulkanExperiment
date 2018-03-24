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

//	vec2 pos01 = constant.pos.xy / u_pm_info.m_resolution.xy;
//	vec2 pos11 = pos01 * 2. - 1.;
	float dist = distance(gl_FragCoord.xy, constant.pos.xy);
//	dist = sqrt(dist * 0.000001);
	float power = 1./(1.+dist);
//	if(power >= 0.5)
	{
		vec2 pos = vec2(gl_FragCoord.xy * u_pm_info.m_resolution.xy);

		int index = atomicAdd(b_emission_counter.x, 1);
		b_emission[index].pos = vec4(gl_FragCoord.xyyy);
		b_emission[index].value = vec4(power);

	}
}