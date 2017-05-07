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

#include <Particle.glsl>
out gl_PerVertex{
	vec4 gl_Position;
};
/*
layout(std140, set=0, binding=0) uniform ParticleInfoUniform 
{
	ParticleInfo u_particle_info;
};
*/
layout(std140, set=0, binding=1) buffer ParticleDataBuffer 
{
	ParticleData b_particle[];
};

layout(std140, set=1, binding=0) uniform CameraUniform
{
	mat4 uProjection;
	mat4 uView;
};

layout(push_constant) uniform UpdateConstantBlock
{
	uint m_src_offset;
} constant;

void main()
{
	ParticleData p = b_particle[gl_InstanceIndex + constant.m_src_offset];
	vec4 pos = vec4(p.m_pos.xyz, 1.0);
	gl_Position = uProjection * uView * vec4(pos.xyz, 1.0);

}
