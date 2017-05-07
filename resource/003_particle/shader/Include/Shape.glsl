struct Triangle
{
	vec3 a;
	vec3 b;
	vec3 c;
};
vec3 getClosestTrianglePoint(in Triangle tri, in vec3 p)
{
	vec3 ab = tri.b - tri.a;
	vec3 ac = tri.c - tri.a;
	vec3 ap = p - tri.a;

	// 点Aの外側の頂点領域の中にあるかチェック
	float d1 = dot(ab, ap);
	float d2 = dot(ac, ap);
	if (d1 <= 0. && d2 <= 0.) {
		return tri.a;
	}

	// 点Bの外側の頂点領域の中にあるかチェック
	vec3 bp = p - tri.b;
	float d3 = dot(ab, bp);
	float d4 = dot(ac, bp);
	if (d3 >= 0. && d4 <= d3) {
		return tri.b;
	}

	float vc = d1*d4 - d3*d2;
	if (vc <= 0. && d1 >= 0. && d3 <= 0.) {
		float v = d1 / (d1 - d3);
		return tri.a + v* ab;
	}

	vec3 cp = p - tri.c;
	float d5 = dot(ab, cp);
	float d6 = dot(ac, cp);
	if (d6 >= 0. && d5 <= d6)
	{
		return tri.c;
	}

	float vb = d5*d2 - d1*d6;
	if (vb <= 0. && d2 >= 0. && d6 <= 0.)
	{
		float w = d2 / (d2 - d6);
		return tri.a + w * ac;
	}

	float va = d3*d6 - d5 *d4;
	if (va <= 0. && d4 - d3 >= 0. && d5 - d6 >= 0.)
	{
		float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return tri.b + w*(tri.c - tri.b);
	}

	// 面の中にある
	float denom = 1. / (va + vb + vc);
	float v = vb*denom;
	float w = vc*denom;
	return tri.a + ab * v + ac * w;

}

float getDistanceTrianglePoint(in Triangle tri, in vec3 p)
{
	return distance(getClosestTrianglePoint(tri, p), p);
}

vec3 getNormalTriangle(in Triangle tri)
{
	return cross(normalize(tri.b - tri.a), glm::normalize(tri.c - tri.a));
}

vec3 getCenterTriangle(in Triangle tri)
{
	return (tri.a + tri.b + tri.c) / 3.;
}
