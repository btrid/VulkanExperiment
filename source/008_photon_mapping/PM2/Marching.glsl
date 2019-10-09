
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
	vec3 tmin = vec3(0.);
	vec3 tmax = vec3(10e6);
	// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
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
	tmin = max(near, tmin);
	tmax = max(far, tmax);

	if (any(greaterThan(tmin, tmax))) {
		return ret;
	}
	float dist = min(min(tmin.x, tmin.y), tmin.z);
#endif
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
#if 1
	// 0除算回避版。不要？
	dist.x = abs(dir.x) < 0.001 ? 999999. : abs(x / dir.x);
	dist.y = abs(dir.y) < 0.001 ? 999999. : abs(y / dir.y);
	dist.z = abs(dir.z) < 0.001 ? 999999. : abs(z / dir.z);
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
	pos += dir * rate + vec3(next)*0.001; // ちょっとずれちゃうけど・・・
	return next;
}

shared bool sIsSkip;
shared bool sIsEnd;
shared int sProcessIndex;
void marching(in BrickParam param)
{
	// 放射されたフォトンを進める
	bool isFirst = gl_LocalInvocationID.x == uint(0);

	vec3 cellSize0 = getCellSize0(param);
	ivec3 cellNum0 = param.num0;
	vec3 cellSize1 = getCellSize1(param);
	ivec3 cellNum1 = param.num1;

	while(true)
	{

		if(gl_LocalInvocationID.x == uint(0))
		{
			sIsEnd = atomicCounter(aMarchingProcessIndex) >= atomicCounter(aPhotonEmitBlockCount);
			if(!sIsEnd)
			{
				sProcessIndex = int(atomicCounterIncrement(aMarchingProcessIndex));
				int offset = (sProcessIndex % (PHOTON_EMIT_BUFFER_SIZE/PHOTON_BLOCK_NUM)) * PHOTON_BLOCK_NUM;
				Photon test = bEmitPhoton[offset];
				ivec3 index0 = getIndexBrick0(param, test.Position);
				sIsSkip = (imageLoad(tBrickMap0, index0).x == 0);
			}

		}

		memoryBarrierShared();
		barrier();

		if(sIsEnd)
		{
			return;
		}

//		int offset = (sProcessIndex % (PHOTON_EMIT_BUFFER_SIZE/PHOTON_BLOCK_NUM)) * PHOTON_BLOCK_NUM;
		int offset = sProcessIndex * PHOTON_BLOCK_NUM;
		Photon photon = bEmitPhoton[offset + gl_LocalInvocationID.x];
		Ray ray;
		ray.p = photon.Position;
		ray.d = getDirection(photon);
		ivec3 index0 = getIndexBrick0(param, ray.p);
		ivec3 index1 = getIndexBrick1(param, ray.p);
		if(sIsSkip)
		{
			// ここには三角形はないのでスキップ
			marchCell(param, photon.Position, index0, ray.d, cellSize0);
			ivec3 nextIndexMap0 = getIndexBrick0(param, photon.Position);
			if(all(greaterThanEqual(nextIndexMap0, ivec3(0))) && all(lessThan(nextIndexMap0, cellNum0)))
			{
				int nextIndex = convert3DTo1D(nextIndexMap0, param.num0);
				StorePhotonBatch(nextIndex, photon);
			}
			continue;
		}
		MarchResult result;
		result.HitResult = MakeHit();
		Hit hitResult = MakeHit();
		TriangleMesh near;

		bool isContinue = true;
		for(; all(equal(index1/param.scale1, index0));)
		{
			int index11D = convert3DTo1D(index1, cellNum1);
			for(uint i = 0; i < bTriangleReferenceNum[index11D]; i++)
			{
				TriangleLL reference = bTriangleReferenceData[index11D*TRIANGLE_BLOCK_NUM+i];
				Vertex a = bVertex[reference.index[0]];
				Vertex b = bVertex[reference.index[1]];
				Vertex c = bVertex[reference.index[2]];
				Triangle t = MakeTriangle(a.Position, b.Position, c.Position);
				t = scaleTriangle(t, 0.001); // 計算誤差でポリゴンの間が当たらないことがあるので大きくする
				Hit hit = intersect(t, ray);

				if(hit.IsHit != 0 && hit.Distance < hitResult.Distance)
				{
					near.a = a;
					near.b = b;
					near.c = c;
					hitResult = hit;
				}
			}			

			if(hitResult.IsHit == 0)
			{
				// 当たらなかったらmarch
				index1 += marchCell(param, photon.Position, index1, ray.d, cellSize1);
				continue;
			}
			// ヒット
			result.HitResult = hitResult;
			result.HitTriangle = near;
			if(!MarchingCallback(result, photon))
			{
				// 続きがないなら終了
				isContinue = false;
				break;
			}
		}

		ivec3 nextIndexMap0 = index1/4;
		if(isContinue
		&& all(greaterThanEqual(nextIndexMap0, ivec3(0)))
		&& all(lessThan(nextIndexMap0, cellNum0)))
		{
			// このセル内ではあたらないので次へ
			int nextIndex = convert3DTo1D(nextIndexMap0, param.num0);
			StorePhotonBatch(nextIndex, photon);
		}

	}
}
