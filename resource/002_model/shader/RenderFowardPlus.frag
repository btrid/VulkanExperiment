#version 450
#pragma optionNV (unroll all)
#pragma optionNV (inline all)

//#extension GL_ARB_bindless_texture : require
//#extension GL_NV_gpu_shader5 : require
//#extension GL_ARB_shading_language_include : require
//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require
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

layout (set = 1, binding = 32) uniform sampler2D tDiffuse;

layout(std140, binding=16) restrict buffer MaterialBuffer {
	Material materials[];
};
layout(std140, binding=17) restrict buffer LightLLHeadBuffer {
	uint b_lightLL_head[];
};
layout(std140, binding=18) restrict buffer LightLLBuffer {
	LightLL b_lightLL[];
};
layout(std140, binding=19) restrict buffer LightBuffer {
	LightParam b_light[];
};

layout(location=0) out vec4 FragColor;
//layout(location=1) out vec4 OutNormal;

vec3 getColor(in Vertex v)
{
//	Material m = materials[FSIn.MaterialIndex];
	gl_FragCoord
	for(uint i = )
	vec3 pos = v.Position;
	vec3 norm = v.Normal;
	vec3 s = normalize(LightPosition.xyz - pos);
	vec3 v = normalize(-pos.xyz);
	vec3 r = reflect( -s, norm );
	vec3 ambient = LightAmbient * (m.AmbientTex != 0 ? texture(sampler2D(m.AmbientTex), FSIn.Texcoord.xy).xyz : m.Ambient.xyz);
	float sDotN = max( dot(s,norm), 0.0 );
	vec3 diffuse = LightDiffuse * (m.DiffuseTex != 0? texture(sampler2D(m.DiffuseTex), FSIn.Texcoord.xy).xyz : m.Diffuse.xyz) * (sDotN + 0.5);

	vec3 diffuse = texture(tDiffuse, FSIn.Texcoord.xy).xyz;
	return diffuse;
}

void main()
{
//	int i = getMortonNumber(FSIn.Position.xyz, 0.);
//	color[i].rgba.rgb = getColor();
//	color[i].rgba.rgb = vec3(1.);
//	color[i].rgba.a = 1.;
	FragColor.rgb = getColor();
	FragColor.a = 1.;
}