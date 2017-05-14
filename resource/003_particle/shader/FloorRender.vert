#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)

#extension GL_ARB_shader_draw_parameters : require

#ifdef VULKAN
#extension GL_GOOGLE_cpp_style_line_directive : require
//#extension GL_KHR_vulkan_glsl : require
#else
#extension GL_ARB_shading_language_include : require
#endif

layout(location = 0)in vec3 inPosition;

out gl_PerVertex{
	vec4 gl_Position;
};

out VertexOut
{
	vec3 Vertex;
}Out;
layout(std140, set=0, binding=0) uniform CameraUniform
{
	mat4 uProjection;
	mat4 uView;
};

void main()
{
	vec4 pos = vec4(inPosition.xyz, 1.0);
	gl_Position = uProjection * uView * pos;
	Out.Vertex = inPosition.xyz;
}
