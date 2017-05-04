struct Plane
{
	vec3 normal;
	float n;
};
struct Frustom{
	Plane p[6];
};
struct Frustom2{
	Plane p[4];
};

Plane MakePlane(in vec3 a, in vec3 b, in vec3 c)
{
	Plane p;
	p.normal = normalize(cross(b - a, c - a));
	p.n = dot(p.normal, a);
	return p;
}

bool isCulling(in Frustom frustom, in vec4 AABB)
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
bool isCulling2(in Frustom2 frustom, in vec4 AABB)
{
	int count = 0;
	for (int i = 0; i < 4; i++)
	{
		float dist = dot(AABB.xyz, frustom.p[i].normal) - frustom.p[i].n;
		if (dist > -abs(AABB.w*2.)) {
			count++;
//			return true;
		}
	}
	return count == 4;
}
