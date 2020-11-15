#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_DC 0
#include "DC.glsl"

layout(location = 0)in vec3 in_position;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out VSVertex
{
	vec3 Position;

}Out;

void main()
{
	gl_Position = vec4(in_position, 1.0);
	Out.Position = gl_Position.xyz;
}
