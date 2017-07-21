

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

ivec3 marchCell(in BrickParam param, inout vec3 pos, in ivec3 cell, in vec3 dir, in vec3 cellSize)
{
	vec3 cellOrigin = cell * cellSize + param.areaMin;
	vec3 p = pos - cellOrigin;
	float x = dir.x < 0. ? p.x : (cellSize.x- p.x);
	float y = dir.y < 0. ? p.y : (cellSize.y- p.y);
	float z = dir.z < 0. ? p.z : (cellSize.z- p.z);
	vec3 dist = vec3(0.);
#if 0
	// 0除算回避版。不要？
	dist.x = abs(dir.x) < 0.0001 ? 9999.9 : abs(x / dir.x);
	dist.y = abs(dir.y) < 0.0001 ? 9999.9 : abs(y / dir.y);
	dist.z = abs(dir.z) < 0.0001 ? 9999.9 : abs(z / dir.z);
#else
	dist = abs(vec3(x, y, z) / dir);
#endif

	ivec3 next = ivec3(0);
	if(dist.x < dist.y && dist.x < dist.z){
		next.x = dir.x < 0. ? -1 : 1;
	}else if(dist.y < dist.z){
		next.y = dir.y < 0. ? -1 : 1;
	}else{
		next.z = dir.z < 0. ? -1 : 1;
	}
	float rate = min(min(dist.x, dist.y), dist.z);
	pos += dir * rate + vec3(next)*0.0001;
	return next;
}

MarchResult marching(in Ray ray)
{
	MarchResult result;
	result.HitResult = MakeHit(); 

	{
		// そもそもVoxelにヒットするかチェック
		Hit start = marchToAABB(ray, param.areaMin, param.areaMax);
		if(start.IsHit == 0 ){
			return result;
		}
		ray.p = start.HitPoint + ray.d*0.0001;
	}

	while()
	{
		Hit hitResult = MakeHit();
		if(hitResult.IsHit == 0)
		{
			// 当たらなかったらmarch
			index1 += marchCell(param, current, index1, ray.d, cellSize1);
			continue;
		}
		// ヒット
		result.HitResult = hitResult;
		result.HitTriangle = near;
		return result;
	}
	return result;
}
