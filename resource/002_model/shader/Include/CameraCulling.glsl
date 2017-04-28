struct Plane
{
	vec3 normal;
	float n;
};
struct Frustom{
	Plane p[6];
};

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
