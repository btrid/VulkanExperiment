#version 450

#extension GL_GOOGLE_include_directive : require

#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

layout(location = 0)in vec3 in_position;

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(push_constant) uniform UpdateConstantBlock
{
	mat4 m_mat;
} constant;


void main()
{
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * constant.m_mat * vec4(in_position.xyz, 1.0);
}
