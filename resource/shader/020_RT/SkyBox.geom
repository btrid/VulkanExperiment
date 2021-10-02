#version 460

#extension GL_GOOGLE_include_directive : require

#define SETPOINT_CAMERA 0
#include "btrlib/camera.glsl"

layout(points, invocations = 1) in;
layout(triangle_strip, max_vertices = 14) out;

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 1) out A
{
	vec3 Texcoord_0;
}Out;

const vec3 cube_strip[] = 
{
	vec3(0.f, 1.f, 1.f),    // Front-top-left
	vec3(1.f, 1.f, 1.f),    // Front-top-right
	vec3(0.f, 0.f, 1.f),    // Front-bottom-left
	vec3(1.f, 0.f, 1.f),    // Front-bottom-right
	vec3(1.f, 0.f, 0.f),    // Back-bottom-right
	vec3(1.f, 1.f, 1.f),    // Front-top-right
	vec3(1.f, 1.f, 0.f),    // Back-top-right
	vec3(0.f, 1.f, 1.f),    // Front-top-left
	vec3(0.f, 1.f, 0.f),    // Back-top-left
	vec3(0.f, 0.f, 1.f),    // Front-bottom-left
	vec3(0.f, 0.f, 0.f),    // Back-bottom-left
	vec3(1.f, 0.f, 0.f),    // Back-bottom-right
	vec3(0.f, 1.f, 0.f),    // Back-top-left
	vec3(1.f, 1.f, 0.f),    // Back-top-right
};

void main() 
{
	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;
	vec3 p = u_camera[0].u_eye.xyz;
	for(int i = 0; i < cube_strip.length(); i++)
	{
		gl_Position = pv * vec4((cube_strip[i]*2.-1.) + p, 1.);
		Out.Texcoord_0 = cube_strip[i]*2.-1.;
		Out.Texcoord_0.y = -Out.Texcoord_0.y;
//		Out.Texcoord_0 = cube_strip[i];
//		Out.Texcoord_0.y = 1.-Out.Texcoord_0.y;

		EmitVertex();
	}
	EndPrimitive();
}
