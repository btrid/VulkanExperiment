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

#define SETPOINT_CAMERA 1
#include <Camera.glsl>
#include <Common.glsl>

#define SETPOINT_BULLET 0
#include <Bullet.glsl>
layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location=1) out VSOUT{
	float life;
}VSOut;


layout(push_constant) uniform UpdateConstantBlock
{
	uint m_offset;
} constant;

void main()
{
	BulletData p = b_bullet[gl_InstanceIndex + constant.m_offset];
	vec3 vertex = vec3(gl_VertexIndex/2 - 0.5, gl_VertexIndex%2 - 0.5, 0.) * p.m_pos.w;
	vec4 pos = vec4(p.m_pos.xyz + vertex, 1.0);
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * pos;
	VSOut.life = 0.2;
}
