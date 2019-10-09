
struct Ray
{
	vec3 p;
	vec3 d;
};

Ray MakeRay(in vec3 p, in vec3 d)
{
	Ray r;
	r.p = p;
	r.d = d;
	return r;
}

struct Triangle
{
	vec3 a, b, c;
};
Triangle MakeTriangle(in vec3 a, in vec3 b, in vec3 c)
{
	Triangle t;
	t.a = a;
	t.b = b;
	t.c = c;
	return t;
}
Triangle scaleTriangleV(in Triangle tri, in vec3 factor) {
	// 適当に
	vec3 l1 = normalize((tri.a - tri.b) + (tri.a - tri.c));
	vec3 l2 = normalize((tri.b - tri.a) + (tri.b - tri.c));
	vec3 l3 = normalize((tri.c - tri.b) + (tri.c - tri.a));
	tri.a += l1*factor.x;
	tri.b += l2*factor.y;
	tri.c += l3*factor.z;
	return tri;
}

Triangle scaleTriangle(in Triangle tri, in float factor) {
	return scaleTriangleV(tri, vec3(factor));
}

Triangle setScaleTriangle(in Triangle tri, in float factor) {
	vec3 l1 = normalize((tri.a - tri.b) + (tri.a - tri.c));
	vec3 l2 = normalize((tri.b - tri.a) + (tri.b - tri.c));
	vec3 l3 = normalize((tri.c - tri.b) + (tri.c - tri.a));
	vec3 center = (tri.a + tri.b + tri.c) / 3.;
	return MakeTriangle(center+l1, center+l2, center+l3);
}

vec3 getNormal(in Triangle t)
{
	vec3 ab = t.b - t.a;
	vec3 ac = t.c - t.a;
	return normalize(cross(ab, ac));
}
float getArea(in Triangle t){
	vec3 ab = t.b - t.a;
	vec3 ac = t.c - t.a;
	return length(cross(ab, ac)) ;
}

//! 縮退三角形？
bool isDegenerate(in Triangle t){
	// 面積が0なら縮退かな
	vec3 ab = t.b - t.a;
	vec3 ac = t.c - t.a;
	return length(cross(ab, ac)) < 0.00001;
}

struct Hit
{
	int IsHit;
	vec3 Weight;
	float Distance;
	vec3 HitPoint;
};

Hit MakeHit()
{
	Hit h;
	h.IsHit = 0;
	h.Distance = 99999.;
	return h;
}

Hit intersect(in Triangle tri, in Ray r)
{
	// p191
	// 縮退3角形はそもそも面積がないので自分でチェックして
	// assert(!isDegenerate());

	Hit hit = MakeHit();
	vec3 ab = tri.b - tri.a;
	vec3 ac = tri.c - tri.a;
	vec3 qp = -r.d;// p - q

	vec3 n = cross(ab, ac);

	float d = dot(qp, n);
	if (d < 0.){
		// 平行か逆向き
		return hit;
	}

	vec3 ap = r.p - tri.a;
	float t = dot(ap, n);
	if (t < 0.){ 
		// 向きが後ろ向き
		return hit;
	}
	vec3 e = cross(qp, ap);
	float v = dot(ac, e);
	if (v <= 0. || v >= d){
		return hit;
	}
	float w = -dot(ab, e);
	if (w <= 0. || v + w >= d){
		return hit;
	}

	float ood = 1. / d;
	t *= ood;
	v *= ood;
	w *= ood;
	float u = 1.- v - w;
	hit.IsHit = 1;
	hit.Weight = vec3(u, v, w);
	hit.Distance = t;
	hit.HitPoint = tri.a*u + tri.b*v + tri.c*w;
	return hit;
}
