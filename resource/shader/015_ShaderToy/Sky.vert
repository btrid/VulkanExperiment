#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_RenderTarget 0
#include "applib/System.glsl"

layout(set=1, binding=0) uniform sampler3D s_map;
layout(set=1, binding=1, r8ui) uniform uimage3D i_map;

layout(location = 0)in vec3 i_position;

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};
layout(location = 1) out Vertex
{
	vec3 Position;
//	vec3 Normal;
//	vec3 Texcoord;
//	flat int DrawID;
}VSOut;

layout(push_constant) uniform Input
{
	mat4 PV;
	vec4 CamPos;
} constant;


void main()
{
	vec3 p = i_position*1030. + vec3(0., -1000., 0.);
	gl_Position = constant.PV * vec4(p, 1.);
	VSOut.Position = p;
//	VSOut.Normal = mat3(skinningMat) * inNormal.xyz;
//	VSOut.Texcoord = inTexcoord.xyz;
//	VSOut.DrawID = gl_DrawID;
}