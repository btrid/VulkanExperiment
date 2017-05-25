

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

MarchResult marching(in BrickParam param, in Ray ray, in readonly uimage3D t_brick_map, float progress_distance)
{
	MarchResult result;
	result.HitResult = MakeHit(); 

	{
		// そもそもVoxelにヒットするかチェック
		Hit start = marchToAABB(ray, param.areaMin, param.areaMax);
//		if(start.IsHit == 0 ){
//			return result;
//		}
		ray.p = start.HitPoint + ray.d*0.0001;
	}

	vec3 cellSize0 = getCellSize0(param);
	ivec3 cellNum0 = param.num0;
	vec3 cellSize1 = getCellSize1(param);
	ivec3 cellNum1 = param.num1;

	vec3 current  = ray.p;
	ivec3 index0 = getIndexBrick0(param, current);
	while(isInRange(index0, cellNum0))
	{
		for(; isInRange(index0, cellNum0) && imageLoad(t_brick_map, index0).x == 0;)
		{
			index0 += marchCell(param, current, index0, ray.d, cellSize0);
		}

		if(!isInRange(index0, cellNum0)){
			return result;
		}

		ivec3 index1 = getIndexBrick1(param, current);
		while(all(greaterThanEqual(index1, ivec3(0))) && all(equal(index1/param.m_subcell_edge_num, index0)))
		{
			Hit hitResult = MakeHit();
			TriangleMesh near;
			for(uint triIndex = bTriangleLLHead[getTiledIndexBrick1(param, index1)]; triIndex != 0xFFFFFFFF; triIndex = bTriangleLL[triIndex].next)
			{
				TriangleLL triangle_LL = bTriangleLL[triIndex];
				triIndex = triangle_LL.next;

				Vertex a = bVertex[triangle_LL.index[0]];
				Vertex b = bVertex[triangle_LL.index[1]];
				Vertex c = bVertex[triangle_LL.index[2]];
				Triangle t = MakeTriangle(a.Position, b.Position, c.Position);
				t = scaleTriangle(t, 0.05); // 計算誤差でポリゴンの間が当たらないことがあるので大きくする
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
				index1 += marchCell(param, current, index1, ray.d, cellSize1);
				continue;
			}
			// ヒット
			result.HitResult = hitResult;
			result.HitTriangle = near;
			return result;
		}

		index0 += marchCell(param, current, index0, ray.d, cellSize0);
	}
	return result;
}
