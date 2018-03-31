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

void main()
{

	float dist = distance(gl_FragCoord.xy, constant.pos.xy);
	{
		ivec2 index2d = ivec2(gl_FragCoord.xy);
		int index = index2d.x + index2d.y*u_pm_info.m_resolution.x;

		// list登録
/*		if(atomicCompSwap(b_emission_map[index], -1, 0) == -1)
		{
			b_emission[index].value = vec4(0.);// 危ないかも？
			int list_index = atomicAdd(b_emission_counter[0].x, 1);
			b_emission_list[list_index] = index;
		}
*/
		if(atomicCompSwap(b_emission_map[index], -1, 0) == -1)
		{
			b_emission[index].value = vec4(0.);// 危ないかも？
			int list_index = atomicAdd(b_emission_counter[0].x, 1);
			b_emission_list[list_index] = index;
		}
		b_emission[index].value += vec4(1500., 0., 0., 0.);
	}
}