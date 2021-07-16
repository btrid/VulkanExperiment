#version 460
#extension GL_GOOGLE_include_directive : require

layout(set=0, binding=0, std140) uniform _Dummy
{
	int a;
};
#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

#define USE_RenderTarget 2
#include "applib/System.glsl"

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};


vec3 _v[] = 
{
	{-1,  1,  1},
	{ 1,  1,  1},
	{-1, -1,  1},
	{ 1, -1,  1},
	{-1,  1, -1},
	{ 1,  1, -1},
	{-1, -1, -1},
	{ 1, -1, -1},
};

uvec3 _i[] =
{
	{0, 2, 3},
	{0, 3, 1},
	{0, 1, 5},
	{0, 5, 4},
	{6, 7, 3},
	{6, 3, 2},
	{0, 4, 6},
	{0, 6, 2},
	{3, 7, 5},
	{3, 5, 1},
	{5, 7, 6},
	{5, 6, 4},
};
void main()
{
	uint id = gl_VertexIndex%3;
	vec3 v = _v[_i[gl_VertexIndex/3][gl_VertexIndex%3]];
	gl_Position = vec4(v*300., 1.0);
	Camera cam = u_camera[0];


	mat3 R = transpose(mat3(cam.u_view));
	vec3 s = R[0];
	vec3 u = R[1];
	vec3 f = R[2];

	vec3 dir = vec3(0.)-cam.u_eye.xyz;

	float fd = dot(dir, f);
	float sd = dot(dir, s);
	float ud = dot(dir, u);

	mat3 h = mat3(
		vec3(1, 0,0),
		vec3(0, 1,0),
		vec3(sd/fd, 0, 1)
	);

	v = h*(dir);
	gl_Position = cam.u_projection * cam.u_view * vec4(v, 1.);
	return;


	mat3 shear = mat3
	(
		vec3(1,0,0),
		vec3(0,1,0),
		vec3(sd/fd,0,1)
	);
/*		auto hf = mat3(
		vec3(1, 0, 0),
		vec3(0, 1, 0),
		vec3(sd/fd, ud/fd, 1)
	);
*/
	gl_Position = cam.u_projection * cam.u_view * vec4(shear * gl_Position.xyz, 1.);
}
