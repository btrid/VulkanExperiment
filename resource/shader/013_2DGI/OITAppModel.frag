#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>

#define USE_PM 0
#include <PM.glsl>

#define USE_MODEL_INFO_SET 1
#include <applib/model/MultiModel.glsl>
/*
in gl_PerFragment{
	vec4 gl_FragCoord;
};
*/
layout(location=1) in ModelData
{
	vec2 texcoord;
}in_modeldata;

void setAlbedo(in vec3 albedo)
{
/*	vec2 vpos = gl_FragCoord.xy / gl_FragCoord.w;
	int index1D = int(vpos.x + vpos.y * u_pm_info.m_resolution.x);
	if(atomicCompSwap(b_fragment_map[index1D], 0, 1) == 0)
	{
//		int fragment_index = atomicAdd(b_fragment_counter, 1);
//		b_fragment[fragment_index].albedo = albedo;
//		b_fragment_map[index1D] = fragment_index;
	}
	b_fragment[index1D].albedo = albedo;
*/}
void setEmission(in vec3 emissive)
{
/*	vec2 vpos = gl_FragCoord.xy / gl_FragCoord.w;
	int index1D = int(vpos.x + vpos.y * u_pm_info.m_resolution.x);
	if(atomicCompSwap(b_emissive_map[index1D], 0, 1) == 0)
	{
		int emissive_index = atomicAdd(b_emissive_counter[0].x, 1);
		b_emissive[emissive_index] = emissive;
		b_emissive_map[index1D] = emissive_index;
	}
*/
}

void main()
{
	vec3 diffuse = texture(tDiffuse[0], in_modeldata.texcoord.xy).xyz;
	setAlbedo(diffuse);
	setEmission(diffuse);
}