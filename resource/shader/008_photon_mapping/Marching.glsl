

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

#if 1
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
#else
//	vec3 tmin = vec3(0.);
//	vec3 tmax = vec3(10e6);
	// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
//	abs(ray.d[i]) < 10e-6
//	if (any(less(ray.p[i], bmin)) || any(greater(ray.p[i], bmax))) 
//	if (ray.p[i] < bmin[i] || ray.p[i] > bmax[i])
//	{
//		return ret;
//	}
	vec3 ood = 1. / ray.d;
	vec3 t1 = (bmin - ray.p) * ood;
	vec3 t2 = (bmax - ray.p) * ood;

	// t1が近い平面との交差、t2が遠い平面との交差になる
	vec3 near = min(t1, t2);
	vec3 far = max(t1, t2);

	// スラブの交差している感覚との交差を計算
//	vec3 tmin = max(near, tmin);
//	vec3 tmax = max(far, tmax);
//	if (any(greaterThan(tmin, tmax))) {
//		return ret;
//	}
//	float dist = min(min(tmin.x, tmin.y), tmin.z);

	float tmin = 0.;
	float tmax = 10e6;
	for (int i = 0; i < 3; i++)
	{
		tmin = max(near[i], tmin);
		tmax = max(far[i], tmax);
	}
	if (tmin > tmax) {
		return ret;
	}
	float dist = tmin;
#endif
	ret.IsHit = 1;
	ret.HitPoint = ray.p + ray.d*dist;
	ret.Distance = dist;
	return ret;

}

MarchResult marching(in Ray ray)
{
	MarchResult result;
	result.HitResult = MakeHit(); 

	{
		// そもそもVoxelにヒットするかチェック
		Hit start = marchToAABB(ray, u_pm_info.area_min, u_pm_info.area_max);
		if(start.IsHit == 0 ){
			return result;
		}
		ray.p = start.HitPoint + ray.d*0.0001;
	}

	vec3 cellSize0 = getCellSize0(param);
	ivec3 cellNum0 = param.num0;
	vec3 cellSize1 = getCellSize1(param);
	ivec3 cellNum1 = param.num1;

	vec3 current  = ray.p;
	ivec3 index0 = getIndexBrick0(param, current);
	for(int i = 0; i < 500; i++)
	{
		for(; isInRange(index0, cellNum0) && imageLoad(tBrickMap0, index0).x == 0;)
		{
			current += ray.d;
		}

		ivec3 index1 = getIndexBrick1(param, current);
		while(all(greaterThanEqual(index1, ivec3(0))) && all(equal(index1/param.scale1, index0)))
		{
			Hit hitResult = MakeHit();
			TriangleMesh near;
			for(uint triIndex = bTriangleLLHead[getTiledIndexBrick1(param, index1)]; triIndex != 0xFFFFFFFF; )
			{
				TriangleLL triangle_LL = bTriangleLL[triIndex];
				triIndex = triangle_LL.next;

				Vertex a = bVertex[triangle_LL.index[0]];
				Vertex b = bVertex[triangle_LL.index[1]];
				Vertex c = bVertex[triangle_LL.index[2]];
				Triangle t = MakeTriangle(a.Position, b.Position, c.Position);
				t = scaleTriangle(t, 0.5); // 計算誤差?でポリゴンの間が当たらないことがあるので大きくする
				Hit hit = intersect(t, ray);

				if(hit.IsHit != 0 && hit.Distance < hitResult.Distance)
				{
					near.a = a;
					near.b = b;
					near.c = c;
					hitResult = hit;
					result.TriangleLLIndex = triIndex;
				}
			}			

			if(hitResult.IsHit == 0)
			{
				// 当たらなかったらmarch
				current += ray.d;
				continue;
			}
			// ヒット
			result.HitResult = hitResult;
			result.HitTriangle = near;
			return result;
		}

		current += ray.d;
	}
	return result;
}
