#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)

#extension GL_ARB_shader_draw_parameters : require

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/Common.glsl>

#define SETPOINT_CAMERA 0
#include <btrlib/Camera.glsl>

layout(location = 0)in vec3 inPosition;

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out vec3 Vertex;
void main()
{
	vec4 pos = vec4(inPosition.xyz, 1.0);
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * pos;
	Vertex = inPosition.xyz;
}
