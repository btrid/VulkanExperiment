#version 450
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#include <btrlib/ConvertDimension.glsl>

#define USE_MODEL_INFO_SET 0
#include <applib/model/MultiModel.glsl>
#include <applib/model/Light.glsl>

layout(early_fragment_tests) in;
layout(origin_upper_left) in vec4 gl_FragCoord;

layout(location = 1) in Vertex 
{
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
	flat int DrawID;
}FSIn;

layout(std140, set=2, binding=0) uniform LightInfoUniform {
	LightInfo u_light_info;
};
layout(std430, set=2, binding=1) readonly restrict buffer LightLLHeadBuffer {
	uint b_lightLL_head[];
};
layout(std430, set=2, binding=2) readonly restrict buffer LightLLBuffer {
	LightLL b_lightLL[];
};
layout(std430, set=2, binding=3) readonly restrict buffer LightBuffer {
	LightParam b_light[];
};

layout(location=0) out vec4 FragColor;

vec3 getColor()
{
	uvec2 tile_index = uvec2(gl_FragCoord.xy / u_light_info.m_tile_size);
	uint tile_index_1D = convert2DTo1D(tile_index, u_light_info.m_tile_num);
	tile_index_1D = 0;
	vec3 pos = FSIn.Position;
	vec3 norm = FSIn.Normal;
	uint material_index = b_material_index[FSIn.DrawID];
	vec3 albedo = texture(tDiffuse[material_index], FSIn.Texcoord.xy).xyz;
	vec3 diffuse = vec3(0.);

	for(uint i = b_lightLL_head[tile_index_1D]; i != INVALID_LIGHT_INDEX;)
	{
		LightLL ll = b_lightLL[i];
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