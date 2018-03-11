
struct CameraPlane
{
	vec3 normal;
	float n;
};
struct CameraFrustom{
	CameraPlane p[6];
};
struct CameraFrustomPoint
{
	vec4 m_ltn;
	vec4 m_rtn;
	vec4 m_lbn;
	vec4 m_rbn;
	vec4 m_ltf;
	vec4 m_rtf;
	vec4 m_lbf;
	vec4 m_rbf;
};

struct Camera
{
	mat4 u_projection;
	mat4 u_view;
	vec4 u_eye;
	vec4 u_target;
	vec4 u_up;
	float u_aspect;
	float u_fov_y;
	float u_near;
	float u_far;

	CameraFrustom u_frustom;
};

#if defined(SETPOINT_CAMERA)
layout(set=SETPOINT_CAMERA, binding=0, std140) uniform CameraUniform
{
	Camera u_camera[2];
};
layout(set=SETPOINT_CAMERA, binding=1, std140) uniform CameraFrustomPointUniform
{
	CameraFrustomPoint u_camera_frustom_point[2];
};
#endif

CameraPlane MakePlane(in vec3 a, in vec3 b, in vec3 c)
{
	CameraPlane p;
	p.normal = normalize(cross(b - a, c - a));
	p.n = dot(p.normal, a);
	return p;
}

bool isCulling(in CameraFrustom frustom, in vec4 AABB)
{
	for (int i = 0; i < 6; i++)
	{
		float dist = dot(AABB.xyz, frustom.p[i].normal) - frustom.p[i].n;
		if (dist < -abs(AABB.w)) {
			return true;
		}
	}
	return false;
}
bool isCullingInf(in CameraFrustom frustom, in vec4 AABB)
{
	int count = 0;
	for (int i = 0; i < 4; i++)
	{
		float dist = dot(AABB.xyz, frustom.p[i].normal) - frustom.p[i].n;
		if (dist < -abs(AABB.w)) {
			return true;
		}
	}
	return false;
}