#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_DC 0
#define USE_Rendering 2
#include "DualContouring.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

layout(location=1) in Data{
	vec3 Normal;
	vec3 Position;
	flat uint VertexIndex;
	flat uint GeometryIndex;
}fs_in;

layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 light = -normalize(vec3(1.));

	float d = dot(fs_in.Normal, light);

	FragColor = vec4(d, d, d, 1.);

	vec3 normal = abs(normalize(fs_in.Normal));
	vec3 albedo = vec3(0.);
	albedo += normal.x * texture(s_albedo[0], fs_in.Position.zy*0.1).xyz;
	albedo += normal.y * texture(s_albedo[0], fs_in.Position.zx*0.1).xyz;
	albedo += normal.z * texture(s_albedo[0], fs_in.Position.xy*0.1).xyz;

	vec3 n = vec3(0.);
	n[fs_in.GeometryIndex] = 1.;
	albedo = n;

	FragColor = vec4(albedo, 1.);

	vec3 g_color[3] = 
	{
		{1,0,0},
		{0,1,0},
		{0,0,1},
	};
//	FragColor = vec4(g_color[fs_in.VertexIndex%3], 1.);
}