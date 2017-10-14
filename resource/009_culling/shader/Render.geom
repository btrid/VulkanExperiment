#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require
//#extension VK_KHR_shader_draw_parameters : require
#extension GL_ARB_viewport_array : require

layout(triangles, invocations = 2) in;
layout(triangle_strip, max_vertices = 3) out;

#define USE_CULLING 0
#include <Culling.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

// built-in
layout(location = 0)in gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
} gl_in[];

layout(location = 0)out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main()
{
		gl_ViewportIndex = gl_InvocationID;
		Camera c = u_camera[gl_InvocationID];
		mat4 PV = c.u_projection * c.u_view;
		for(int i = 0; i<3; i++)
		{
			gl_Position = PV * gl_in[i].gl_Position;
			EmitVertex();
		}

		EndPrimitive();

}
