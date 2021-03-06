#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#define USE_CULLING 0
#include <Culling.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

layout(location = 0)in vec4 inPosition;

layout(location = 0)out gl_PerVertex{
	vec4 gl_Position;
};


void main()
{
	gl_Position = b_world[b_instance_index_map[gl_InstanceIndex]] * vec4(inPosition.xyz, 1.);
//	gl_Position = b_world[gl_InstanceIndex] * vec4(inPosition.xyz, 1.);
}
