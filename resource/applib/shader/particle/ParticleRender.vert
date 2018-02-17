#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/Common.glsl>
#define USE_PARTICLE 0
#include <applib/Particle.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

#define USE_SYSTEM 2
#include <applib/System.glsl>


layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location=1) out VSOUT{
	vec4 m_color;
}VSOut;

void main()
{
	ParticleData p = b_particle[gl_InstanceIndex + (u_system_data.m_gpu_index)*u_particle_info.m_particle_max_num];
	vec3 v = vec3(gl_VertexIndex/2, gl_VertexIndex%2, 0.);
	mat3 invView = inverse(mat3(u_camera[0].u_view));
	v = (invView*v).xyz;

	vec4 pos = vec4(p.m_position.xyz + v, 1.0);
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * pos;
	VSOut.m_color = p.m_color;


}
