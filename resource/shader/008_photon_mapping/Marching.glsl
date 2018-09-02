/*
ivec3 getIndexBrick0(in BrickParam param, in vec3 pos)
{
	vec3 cell = getCellSize0(param);
	return ivec3((pos - param.areaMin) / cell);
}

int getTiledIndexBrick1(in BrickParam param, in ivec3 index1)
{
	ivec3 index0 = index1 / param.scale1;
	ivec3 index01 = index1 - index0*param.scale1;
	int z = convert3DTo1D(index01, ivec3(param.scale1));
	return convert3DTo1D(index0, param.num0)*param.scale1*param.scale1*param.scale1 + z;
}
*/
MarchResult marching(in Ray ray)
{
	MarchResult result;
	result.HitResult = MakeHit(); 

	{
		// そもそもVoxelにヒットするかチェック
		Hit start = marchToAABB(ray, u_pm_info.area_min.xyz, u_pm_info.area_max.xyz);
		if(start.IsHit == 0 ){
			return result;
		}
		ray.p = start.HitPoint + ray.d*0.0001;
	}

//	vec3 cellSize0 = getCellSize0(param);
//	ivec3 cellNum0 = param.num0;
//	vec3 cellSize1 = getCellSize1(param);
//	ivec3 cellNum1 = param.num1;
	vec3 current  = ray.p;
	vec3 inv_dir;
	inv_dir.x = abs(ray.d.x) < 0.0001 ? 9999999. : abs(1./ray.d.x);
	inv_dir.y = abs(ray.d.y) < 0.0001 ? 9999999. : abs(1./ray.d.y);
	inv_dir.z = abs(ray.d.z) < 0.0001 ? 9999999. : abs(1./ray.d.z);
	vec3 dir = ray.d * min(inv_dir.z, min(inv_dir.x, inv_dir.y)) * u_pm_info.cell_size.xyz;
	
	for(int i = 0; i < 500; i++)
	{
		ivec3 index0 = ivec3(current / u_pm_info.cell_size.xyz);
		ivec3 index1 = index0 >> 2;
		uint hierarchy_index = convert3DTo1D(index1, u_pm_info.num0.xyz>>2);
		uint64_t hierarchy = b_triangle_hierarchy[hierarchy_index];
		if(hierarchy == 0)
		{
			while(all(equal(index0>>2, index1)))
			{
				current += dir;
				index0 = ivec3(current / u_pm_info.cell_size.xyz);
			}
			continue;
		}

		while(all(greaterThanEqual(index0, ivec3(0))) && all(equal(index0>>2, index1)))
		{
			Hit hitResult = MakeHit();
			TriangleMesh near;

			for(uint triIndex = bTriangleLLHead[convert3DTo1D(index0, u_pm_info.num0.xyz)]; triIndex != 0xFFFFFFFF; )
			{
				TriangleLL triangle_LL = bTriangleLL[triIndex];
				triIndex = triangle_LL.next;

				Vertex a = b_vertex[triangle_LL.index[0]];
				Vertex b = b_vertex[triangle_LL.index[1]];
				Vertex c = b_vertex[triangle_LL.index[2]];
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
				current += dir;
				index0 = ivec3(current / u_pm_info.cell_size.xyz);
				continue;
			}
			// ヒット
			result.HitResult = hitResult;
			result.HitTriangle = near;
			return result;
		}
	}
	return result;
}
