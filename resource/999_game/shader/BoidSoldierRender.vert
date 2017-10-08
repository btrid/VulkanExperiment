#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <Common.glsl>

#define SETPOINT_BOID 0
#include <Boid.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

#define USE_SYSTEM 2
#include <applib/System.glsl>

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
	float gl_PointSize;
};
layout(location=1) out VSOUT{
	float life;
}VSOut;


void main()
{
	uint offset = u_system_data.m_gpu_index * u_boid_info.m_soldier_max/2;
	vec3 s_pos = b_soldier[gl_VertexIndex + offset].m_pos.xyz;
	vec4 pos = vec4(s_pos.xyz + vec3(0., 0.1, 0.), 1.0);
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * pos;
	gl_PointSize = 300./ gl_Position.w;
	VSOut.life = 0.2;

}
