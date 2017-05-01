
struct LightInfo
{
	uvec2 m_resolution;
	uvec2 m_tile_size;

	uvec2 m_tile_num;
    uint m_active_light_num;

};

struct LightParam
{
	vec4 m_position;
	vec4 m_emission;
};

struct Plane
{
	vec3 N;
	float D;
};

struct Frustom2
{
	Plane p[4];
};

struct FrustomPoint
{
	// compute the 4 corners of the frustum on the near plane
	vec3 m_ltn;
	vec3 m_rtn;
	vec3 m_lbn;
	vec3 m_rbn;
	// compute the 4 corners of the frustum on the far plane
	vec3 m_ltf;
	vec3 m_rtf;
	vec3 m_lbf;
	vec3 m_rbf;
};

Plane MakePlane(in vec3 a, in vec3 b, in vec3 c)
{
	Plane p;
	p.N = normalize(cross(b - a, c - a));
	p.D = dot(p.N, a);
	return p;
}

bool isCulling2(in Frustom2 frustom, in vec4 sphere)
{
	for (int i = 0; i < 4; i++)
	{
		float dist = dot(sphere.xyz, frustom.p[i].N) - frustom.p[i].D;
		if (dist < -abs(sphere.w)) {
			return true;
		}
	}
	return false;
}

