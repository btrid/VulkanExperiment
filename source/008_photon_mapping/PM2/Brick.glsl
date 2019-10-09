
struct BrickParam
{
	ivec3 num0;
	float p1;

	ivec3 num1;
	int scale1;

	vec3 areaMin;
	float p3;

	vec3 areaMax;
	float p4;
};

ivec3 getTiledIndexBrick1(in BrickParam param, in ivec3 index1)
{
#ifdef TILED_MAP
	ivec3 index0 = index1 / param.scale1;
	ivec3 index01 = index1 - index0*param.scale1;
	int z = convert3DTo1D(index01, ivec3(param.scale1));
	return index0 * ivec3(1, 1, param.scale1*param.scale1*param.scale1) + ivec3(0, 0, z);
#else
	return index1;
#endif
}
vec3 getCellSize0(in BrickParam param)
{
	return (param.areaMax - param.areaMin) / param.num0;	
}
vec3 getCellSize1(in BrickParam param)
{
	return (param.areaMax - param.areaMin) / param.num1;	
}
ivec3 getIndexBrick0(in BrickParam param, in vec3 pos)
{
	vec3 cell = getCellSize0(param);
	return ivec3((pos - param.areaMin) / cell);
}
ivec3 getIndexBrick1(in BrickParam param, in vec3 pos)
{
	vec3 cell = getCellSize1(param);
	return ivec3((pos - param.areaMin) / cell);
}

bool isInRange(in ivec3 index, in ivec3 size)
{
	return all(lessThan(index, size)) && all(greaterThanEqual(index, ivec3(0)));
}

