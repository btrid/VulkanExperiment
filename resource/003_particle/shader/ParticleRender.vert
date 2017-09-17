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

#include <Common.glsl>

#define SETPOINT_PARTICLE 0
#include </Particle.glsl>

#define SETPOINT_CAMERA 1
#include <Camera.glsl>

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location=1) out VSOUT{
	vec4 m_color;
}VSOut;

layout(push_constant) uniform UpdateConstantBlock
{
	uint m_offset;
} constant;

void main()
{
	ParticleData p = b_particle[gl_InstanceIndex + constant.m_offset];
	vec3 v = vec3(gl_VertexIndex/2, gl_VertexIndex%2, 0.);
	mat3 invView = inverse(mat3(u_camera[0].u_view));
	v = (invView*v).xyz;

	vec4 pos = vec4(p.m_position.xyz + v, 1.0);
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * pos;
	VSOut.m_color = p.m_color;

}
