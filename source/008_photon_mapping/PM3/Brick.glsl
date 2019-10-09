
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


#if 1
#define getCellSize0(_p) BRICK_CELL_SIZE
#define getCellSize1(_p) BRICK_SUB_CELL_SIZE
#define getIndexBrick0(_p, pos) ivec3((pos - BRICK_AREA_MIN) / BRICK_CELL_SIZE)
#define getIndexBrick1(_p, pos) ivec3((pos - BRICK_AREA_MIN) / BRICK_SUB_CELL_SIZE)
#define getTiledIndexBrick1(_p, index1) \
	convert3DTo1D(index1 / BRICK_SCALE1, ivec3(BRICK_SIZE))*BRICK_SCALE1*BRICK_SCALE1*BRICK_SCALE1 \
	+ convert3DTo1D(index1 - ((index1 / BRICK_SCALE1)*BRICK_SCALE1), ivec3(BRICK_SCALE1))

#else

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
int getTiledIndexBrick1(in BrickParam param, in ivec3 index1)
{
	ivec3 index0 = index1 / param.scale1;
	ivec3 index01 = index1 - index0*param.scale1;
	int z = convert3DTo1D(index01, ivec3(param.scale1));
	return convert3DTo1D(index0, param.num0)*param.scale1*param.scale1*param.scale1 + z;
}

#endif

bool isInRange(in ivec3 index, in ivec3 size)
{
	return all(lessThan(index, size)) && all(greaterThanEqual(index, ivec3(0)));
}

