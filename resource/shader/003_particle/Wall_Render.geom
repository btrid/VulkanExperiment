#version 460

#extension GL_GOOGLE_include_directive : require
#define SETPOINT_CAMERA 0
#include "btrlib/camera.glsl"

#define Use_Fluid 1
#include "Fluid.glsl"

layout(points, invocations = 1) in;
layout(triangle_strip, max_vertices = 14) out;

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

layout(location=1) in Vertex
{
	flat int VertexIndex;
}in_param[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};


void main() 
{
	int vi = in_param[0].VertexIndex;
	if(b_WallEnable[vi] == 0) { return; }

	ivec3 reso = u_constant.GridCellNum;
	int x = vi % reso.x;
	int y = (vi / reso.x) % reso.y;
	int z = (vi / reso.x / reso.y) % reso.z;
	float scale = u_constant.GridCellSize;

	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;

	for(int i = 0; i < cube_strip.length(); i++)
	{
		vec3 p = vec3(x, y, z)*scale; - u_constant.GridMin;
		p += cube_strip[i]*scale*0.5 - scale*0.5;
		gl_Position = pv * vec4(p, 1.);
		EmitVertex();
	}
	EndPrimitive();
}
