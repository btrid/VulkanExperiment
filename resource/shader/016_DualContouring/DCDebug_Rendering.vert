#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_Model 0
#include "DualContouring.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

layout(location=1) out VSVertex
{
	flat uvec3 CellID;
}Out;

 vec3 box[] = 
 {
	{0., 1., 1.},
	{1., 1., 1.},
	{0., 0., 1.},
	{1., 0., 1.},
	{0., 1., 0.},
	{1., 1., 0.},
	{0., 0., 0.},
	{1., 0., 0.},
};

uint box_face[] =
{
	0, 2, 3,
	0, 3, 1,
	0, 1, 5,
	0, 5, 4,
	6, 7, 3,
	6, 3, 2,
	0, 4, 6,
	0, 6, 2,
	3, 7, 5,
	3, 5, 1,
	5, 7, 6,
	5, 6, 4,
};

#define quat vec4
quat makeQuat(vec3 u, vec3 v)
{
	float norm_u_norm_v = sqrt(dot(u, u) * dot(v, v));
	float real_part = norm_u_norm_v + dot(u, v);
	vec3 t;
	if(real_part < 1.e-6f * norm_u_norm_v)
	{
		real_part = 0.;
		t = abs(u.x) > abs(u.z) ? vec3(-u.y, u.x, 0.) : vec3(0., -u.z, u.y);
	}
	else
	{
		t = cross(u, v);
	}
	return normalize(quat(t.x, t.y, t.z, real_part));
}

vec3 mul(quat q, vec3 v)
{
	vec3 uv = cross(q.xyz, v);
	vec3 uuv = cross(q.xyz, uv);

	return v + ((uv * q.w) + uuv) * 2.;
}

void main()
{
	vec3 _axis[3] = { vec3(1., 0., 0.), vec3(0., 1., 0.),vec3(0., 0., 1.) };
	vec3 dir = normalize(vec3(1.f));
	quat rot = makeQuat(_axis[2], dir);
	vec3 f = mul(rot, _axis[2]);
	vec3 s = mul(rot, _axis[0]);
	vec3 u = mul(rot, _axis[1]);

	vec3 extent = vec3(1.f);
	vec3 min = vec3(0.f);
	vec3 center = (extent-min) * 0.5;
	vec3 min2 = mul(rot, (min - center)) + center;
	
vec3 _box[] = 
{
	u+f,
	s+u+f,
	f,
	s+f,
	u,
	s+u,
	vec3(0.),
	s,
};

	vec3 p = box[box_face[gl_VertexIndex]] + min;
//	vec3 p = _box[box_face[gl_VertexIndex]] + min2;
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4(p*Voxel_Block_Size, 1.0);

}
