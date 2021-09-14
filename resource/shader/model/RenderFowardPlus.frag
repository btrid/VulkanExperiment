#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_AppModel 0
#define USE_AppModel_Render 1
#include "applib/AppModel.glsl"

#include "btrlib/ConvertDimension.glsl"

#define USE_LIGHT 3
#include "applib/Light.glsl"

layout(early_fragment_tests) in;
layout(origin_upper_left) in vec4 gl_FragCoord;

layout(location = 1) in Vertex 
{
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
	flat int DrawID;
}FSIn;


layout(location=0) out vec4 FragColor;

vec3 getColor()
{
	uvec2 tile_index = uvec2(gl_FragCoord.xy / u_light_info.m_tile_size);
	uint tile_index_1D = convert2DTo1D(tile_index, u_light_info.m_tile_num);
	tile_index_1D = 0;
	vec3 pos = FSIn.Position;
	vec3 norm = FSIn.Normal;
	uint material_index = b_material_index[FSIn.DrawID];
	vec3 albedo = texture(t_base[material_index], FSIn.Texcoord.xy).xyz;
	vec3 diffuse = vec3(0.);

	for(uint i = b_light_LL_head[tile_index_1D]; i != INVALID_LIGHT_INDEX;)
	{
		LightLL ll = b_light_LL[i];
		LightParam light = b_light[ll.light_index];
		vec3 dir = normalize(light.m_position.xyz - pos);

		float d = max(dot(dir, norm), 0.);
		float dist = distance(pos, light.m_position.xyz);
		diffuse += (light.m_position.w /pow(dist, 1.2)) * light.m_emission.xyz * albedo * min(d+0.01, 1.);
		i = ll.next;
	}

	vec3 ambient = albedo * vec3(0.1);
	return diffuse;
}

void main()
{
	FragColor.rgb = getColor();
//	FragColor.rgb = vec3(1.);
	FragColor.a = 1.;
}