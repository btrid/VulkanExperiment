#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <Common.glsl>

#define SETPOINT_BULLET 0
#include <Bullet.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

#define USE_SYSTEM 2
#include <applib/System.glsl>

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location=1) out VSOUT{
	float life;
}VSOut;


void main()
{
	BulletData p = b_bullet[gl_InstanceIndex + u_system_data.m_gpu_index * u_bullet_info.m_max_num];
	vec3 vertex = vec3(gl_VertexIndex/2 - 0.5, gl_VertexIndex%2 - 0.5, 0.) * p.m_pos.w;
	vec4 pos = vec4(p.m_pos.xyz + vertex, 1.0);
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * pos;
	VSOut.life = 0.2;
}
