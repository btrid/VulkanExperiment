#version 460
#extension GL_GOOGLE_include_directive : require

layout(location = 0)in vec3 in_position;
layout(location = 1)in vec3 in_normal;
layout(location = 2)in vec2 in_texcoord_0;

#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};
layout(location = 1) out Vertex
{
	vec3 WorldPos;
	vec3 Normal;
	flat uvec2 MaterialID;
}Out;

void main()
{
	Camera cam = u_camera[0];
	gl_Position = cam.u_projection * cam.u_view * vec4(in_position, 1.);

	Out.WorldPos = in_position;
	Out.Normal = transpose(inverse(mat3x3(cam.u_view))) * in_normal;
	Out.MaterialID = uvec2(in_texcoord_0+0.5);
//	v_frontColor = vec4(colorMap[int(a_texcoord.x)]);
//	v_backColor = vec4(colorMap[int(a_texcoord.y)]);
}
