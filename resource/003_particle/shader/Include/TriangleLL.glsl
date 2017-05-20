struct BrickParam
{
	uint m_cell_num;
	uint m_cell_num_total;
	uint m_subcell_num_per_cell;	//!< セル内のセルの数
	uint m_subcell_num_total;
	vec4 m_area_min;
	vec4 m_area_max;

	// 事前計算
	vec4 m_cell_size;
	vec4 m_subcell_size;
};

struct MarchResult{
	Hit HitResult;
//	TriangleMesh HitTriangle;
	uint TriangleLLIndex;
};


struct TriangleLL
{
	uint next;
	uint drawID;
	uint instanceID;
	uint _p;
	uint index[3];
	uint _p2;
};

float getArea(in vec3 a, in vec3 b, in vec3 c)
{
	return length(cross(b - a, c - a)) * 0.5;
}
float rand(in vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

#if 0
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
	return param.m_cell_size.xyz;	
}
vec3 getCellSize1(in BrickParam param)
{
	return param.m_subcell_size.xyz;
}
ivec3 getIndexBrick0(in BrickParam param, in vec3 pos)
{
	return ivec3(0);
//	vec3 cell = getCellSize0(param);
//	return ivec3((pos - param.m_area_min.xyz) / cell);
}
ivec3 getIndexBrick1(in BrickParam param, in vec3 pos)
{
	vec3 cell = getCellSize1(param);
	return ivec3((pos - param.m_area_min.xyz) / cell);
}
uint getTiledIndexBrick1(in BrickParam param, in ivec3 index1)
{
	uvec3 index0 = index1 / param.m_subcell_num_per_cell;
	uvec3 index01 = index1 - index0*param.m_subcell_num_per_cell;
	uint z = convert3DTo1D(index01, uvec3(param.m_subcell_num_per_cell));
	return convert3DTo1D(index0, uvec3(param.m_cell_num))*param.m_subcell_num_per_cell*param.m_subcell_num_per_cell*param.m_subcell_num_per_cell + z;
}

#endif

bool isInRange(in uvec3 index, in uvec3 size)
{
	return all(lessThan(index, size)) && all(greaterThanEqual(index, ivec3(0)));
}

