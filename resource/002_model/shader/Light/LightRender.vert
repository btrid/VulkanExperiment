#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)

#extension GL_ARB_shader_draw_parameters : require
//#extension GL_ARB_bindless_texture : require


#ifdef VULKAN
#extension GL_GOOGLE_cpp_style_line_directive : require
//#extension GL_KHR_vulkan_glsl : require
#else
#extension GL_ARB_shading_language_include : require
#endif
//#include </Math.glsl>
#include </Light.glsl>

layout(location = 0)in vec3 inPosition;

out gl_PerVertex{
	vec4 gl_Position;
};

struct Vertex
{
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
};
layout(location = 0) out Vertex VSOut;

layout(std140, set=2, binding=0) uniform CameraUniform
{
	mat4 uProjection;
	mat4 uView;
};
layout(std430, binding=8) readonly restrict buffer LightBuffer {
	LightParam b_light[];
};

void main()
{
	LightParam light = b_light[gl_instanceID];
	vec4 pos = vec4((inPosition + light.m_position).xyz, 1.0);
	gl_Position = uProjection * uView * vec4(pos.xyz, 1.0);

}
