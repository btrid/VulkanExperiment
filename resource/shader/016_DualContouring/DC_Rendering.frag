#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LDC 0
#include "LDC.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

layout(location=1) in Data{
	vec3 Normal;
	flat uint VertexIndex;
}fs_in;

layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 light = -normalize(vec3(1.));

	float d = dot(fs_in.Normal, light);

	FragColor = vec4(d, d, d, 1.);

	FragColor = vec4(normalize(fs_in.Normal), 1.);

	vec3 g_color[3] = 
	{
		{1,0,0},
		{0,1,0},
		{0,0,1},
	};
//	FragColor = vec4(g_color[fs_in.VertexIndex%3], 1.);
}