#ifndef MARCHING_GLSL_
#define MARCHING_GLSL_

#include <>
Ray MakeRayFromScreen(in vec3 eye, in vec3 target, in vec2 screen, in float fov_y, in float aspect)
{
	vec3 foward = normalize(target - eye).xyz;
	vec3 side = cross(vec3(0., 1., 0.), foward);
	side = dot(side, side) <= 0.1 ? vec3(1., 0., 0.) : normalize(side);
	vec3 up = normalize(cross(side, foward));

	// イメージセンサー上の位置
	float tan_fov_y = tan(fov_y / 2.);

	vec3 dir = normalize(foward + side * -screen.x * tan_fov_y * aspect + up * screen.y * tan_fov_y);
	vec3 pos = eye.xyz;
	return MakeRay(pos, dir);
}

Hit marchToAABB(in Ray ray, in vec3 bmin, in vec3 bmax)
{
	Hit ret = MakeHit();

	if(all(lessThan(ray.p, bmax)) 
	&& all(greaterThan(ray.p, bmin)))
	{
		// AABBの中にいる
		ret.IsHit = 1;
		ret.HitPoint = ray.p;
		ret.Distance = 0;
		return ret;
	}

	float tmin = 0.;
	float tmax = 10e6;
	for (int i = 0; i < 3; i++)
	{
		if (abs(ray.d[i]) < 10e-6)
		{
			// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
			if (ray.p[i] < bmin[i] || ray.p[i] > bmax[i])
			{
				return ret;
			}
		}
		else
		{
			float ood = 1. / ray.d[i];
			float t1 = (bmin[i] - ray.p[i]) * ood;
			float t2 = (bmax[i] - ray.p[i]) * ood;

			// t1が近い平面との交差、t2が遠い平面との交差になる
			float near = min(t1, t2);
			float far = max(t1, t2);

			// スラブの交差している感覚との交差を計算
			tmin = max(near, tmin);
			tmax = max(far, tmax);

			if (tmin > tmax) {
				return ret;
			}
		}
	}
	float dist = tmin;
	ret.IsHit = 1;
	ret.HitPoint = ray.p + ray.d*dist;
	ret.Distance = dist;
	return ret;

}

bool isInAABB(in vec3 p, in vec3 bmin, in vec3 bmax)
{
	return all(lessThan(p, bmax)) && all(greaterThan(p, bmin));
}

#endif