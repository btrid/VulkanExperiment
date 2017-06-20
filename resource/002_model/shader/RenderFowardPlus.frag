#version 450
#pragma optionNV (unroll none)
#pragma optionNV (inline all)

//#extension GL_ARB_bindless_texture : require
//#extension GL_NV_gpu_shader5 : require
//#extension GL_ARB_shading_language_include : require
//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#define USE_MESH_SET 1
#include </MultiModel.glsl>
#include </Light.glsl>
#include </ConvertDimension.glsl>

layout(early_fragment_tests) in;
layout(origin_upper_left) in vec4 gl_FragCoord;

struct Vertex
{
//	flat int MaterialIndex;
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
};
layout(location = 0) in Vertex FSIn;

layout(push_constant) uniform ConstantBlock
{
	uint m_material_index;
} constant;




layout(std140, set=3, binding=0) uniform LightInfoUniform {
	LightInfo u_light_info;
};
layout(std430, set=3, binding=1) readonly restrict buffer LightLLHeadBuffer {
	uint b_lightLL_head[];
};
layout(std430, set=3, binding=2) readonly restrict buffer LightLLBuffer {
	LightLL b_lightLL[];
};
layout(std430, set=3, binding=3) readonly restrict buffer LightBuffer {
	LightParam b_light[];
};

layout(location=0) out vec4 FragColor;

vec3 getColor(in Vertex v)
{
	uvec2 tile_index = uvec2(gl_FragCoord.xy / u_light_info.m_tile_size);
	uint tile_index_1D = convert2DTo1D(tile_index, u_light_info.m_tile_num);
	tile_index_1D = 0;
	vec3 pos = v.Position;
	vec3 norm = v.Normal;
	vec3 albedo = texture(tDiffuse, FSIn.Texcoord.xy).xyz;
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
//	FragColor.rgb = getColor(FSIn);
	FragColor.rgb = vec3(1.);
	FragColor.a = 1.;
}