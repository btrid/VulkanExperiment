#version 460
#extension GL_GOOGLE_include_directive : require

layout(location = 0)in vec3 in_position;
layout(location = 1)in int in_type;

#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};
layout(location = 1) out Vertex
{
	vec3 WorldPos;
	flat int Type;
}Out;

void main()
{
	if(in_type == 0)
	{
		gl_Position = vec4(0.);
		gl_PointSize = 0.;
	}
	Camera cam = u_camera[0];
	gl_Position = cam.u_projection * cam.u_view * vec4(in_position, 1.);
	if(in_type==1)
	{
		gl_PointSize = 100. / gl_Position.w;
	}
	if(in_type==2)
	{
		gl_PointSize = 1. / gl_Position.w;
	}

	Out.WorldPos = in_position;
	Out.Type = in_type;
}
