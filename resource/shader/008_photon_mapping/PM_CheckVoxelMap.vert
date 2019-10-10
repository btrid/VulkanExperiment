#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_PM 0
#include "PM.glsl"

layout(location = 0)in vec3 in_position;
layout(location = 1)in int in_material_index;
layout(location = 2)in vec2 in_texcoord0;
layout(location = 3)in vec2 in_texcoord1;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out VSVertex
{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID;
}Out;

void main()
{
	gl_Position = vec4(in_position, 1.0);
	Out.Position = gl_Position.xyz;
	Out.DrawID = gl_DrawID;
	Out.InstanceID = gl_InstanceIndex;
	Out.VertexID = gl_VertexIndex;
}
