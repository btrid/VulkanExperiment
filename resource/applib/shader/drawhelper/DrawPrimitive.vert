#version 450


#pragma optionNV (unroll all)
#pragma optionNV (inline all)
//#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_GOOGLE_cpp_style_line_directive : require

#define SETPOINT_CAMERA 0
#include <Camera.glsl>

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
	gl_Position = u_camera.u_projection * u_camera.u_view * constant.m_mat * vec4(in_position.xyz, 1.0);
}
