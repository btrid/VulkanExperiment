#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#define USE_MODEL_INFO_SET 0
#include <applib/model/MultiModel.glsl>

//#define SETPOINT_VOXEL 2
//#include <btrlib/Voxelize/Voxelize.glsl>

#define USE_LIGHT 2
#include <Light.glsl>

layout(early_fragment_tests) in;

struct Vertex
{
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
};
layout(location = 0) in Vertex FSIn;
//in vec4 gl_FragCoord;
layout(location=0) out vec4 FragColor;

vec3 getColor()
{
	vec3 albedo = texture(tDiffuse[0], FSIn.Texcoord.xy).xyz;
	vec3 pos = FSIn.Position;
	vec3 normal = normalize(FSIn.Normal);

	vec2 screen = (gl_FragCoord.xy/ u_tile_info.m_resolusion);
//	vec2 screen = gl_FragCoord.xy;
	screen = 1. - screen;
	uvec2 tile_index = uvec2(screen * u_tile_info.m_tile_num);
	uint tile_index_1d = tile_index.x + tile_index.y *u_tile_info.m_tile_num.x;
	uint light_num = b_tile_data_counter[tile_index_1d];
	uint light_data_offset = tile_index_1d*u_tile_info.m_tile_index_map_max;

	vec3 diffuse = vec3(0.);
//	#define light b_light[b_tile_data_map[i+light_data_offset]]
	for(uint i = 0 ; i < light_num; i++)
	{
		LightData light = b_light[b_tile_data_map[i+light_data_offset]];
		vec3 s = normalize(light.m_pos.xyz - pos);
		float sDotN = max( dot(s,normal), 0.0 );
		float dist = distance(light.m_pos.xyz, pos) - light.m_pos.w;
		diffuse += sDotN * albedo * light.m_emissive.xyz / (dist*dist);
	}
//	#undef light

	return diffuse;
}

void main()
{
	FragColor.rgb = getColor();
	FragColor.a = 1.;
}