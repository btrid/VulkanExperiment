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
#include <Common.glsl>

#define SETPOINT_BOID 0
#include <Boid.glsl>
out gl_PerVertex{
	vec4 gl_Position;
	float gl_PointSize;
};
out VSOUT{
	float life;
}VSOut;


layout(push_constant) uniform UpdateConstantBlock
{
	uint m_offset;
} constant;

void main()
{
	SoldierData s = b_soldier[gl_VertexIndex + constant.m_offset];
	vec4 pos = vec4(s.m_pos.xyz + vec3(0., 0.1, 0.), 1.0);
	gl_Position = uProjection * uView * pos;
	gl_PointSize = 300./ gl_Position.w;
	VSOut.life = 0.2;
}
