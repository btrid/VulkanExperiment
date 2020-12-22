#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LDC 0
#define USE_Rendering 2
#include "LDC.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

layout(location=1) in Data{
	vec3 Normal;
	flat uint VertexIndex;
	vec3 Position;
}fs_in;

layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 light = -normalize(vec3(1.));

	float d = dot(fs_in.Normal, light);

	FragColor = vec4(d, d, d, 1.);

	vec3 normal = normalize(fs_in.Normal);
	vec3 albedo = vec3(0.);
	albedo += abs(dot(vec3(1.,0.,0.), normal)) * texture(s_albedo[0], fs_in.Position.yz*0.01).xyz;
	albedo += abs(dot(vec3(0.,1.,0.), normal)) * texture(s_albedo[0], fs_in.Position.zx*0.01).xyz;
	albedo += abs(dot(vec3(0.,0.,1.), normal)) * texture(s_albedo[0], fs_in.Position.xy*0.01).xyz;

	FragColor = vec4(albedo, 1.);


	vec3 g_color[3] = 
	{
		{1,0,0},
		{0,1,0},
		{0,0,1},
	};
//	FragColor = vec4(g_color[fs_in.VertexIndex%3], 1.);
}