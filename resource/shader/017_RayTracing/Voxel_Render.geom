#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"

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

float sdBox( vec3 p, vec3 b )
{
	vec3 d = abs(p) - b;
	return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}
float sdSphere( vec3 p, float s )
{
  return length( p ) - s;
}

float map(in vec3 p)
{
	float d = 9999999.;
	{
		d = min(d, sdSphere(p-vec3(1000., 250., 1000.), 1000.));
	}
	return d;
}

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
	int x = vi % u_info.reso.x;
	int y = (vi / u_info.reso.x) % u_info.reso.y;
	int z = (vi / u_info.reso.x / u_info.reso.y) % u_info.reso.z;

	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;
	vec4 check = pv * vec4(vec3(x, y, z), 1.);
	check /= check.w;
	if(any(lessThan(check.xy, vec2(-1.))) || any(greaterThan(check.xy, vec2(1.))))
	{
		return;
	}

	vec3 p = vec3(x, y, z)+0.5;
	vec2 d = vec2(0.);
	d[0] = map(p);
	d[1] = map(p+1.);

	// 境界のみボクセル化する
	if(any(greaterThanEqual(d, vec2(0))) && any(lessThanEqual(d, vec2(0))))
	{
	}
	else
	{
		return;
	}

	for(int i = 0; i < cube_strip.length(); i++)
	{
		gl_Position = pv * vec4(vec3(cube_strip[i]+vec3(x, y, z)), 1.);
		EmitVertex();
	}
	EndPrimitive();
}
