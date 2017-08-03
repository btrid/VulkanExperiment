struct BrickParam
{
	uvec4	m_cell_num;	//!< xyz:1d w:x*y*z 
	uvec4	m_subcell_num;	//!< セル内のセルの数

	uvec4 m_total_num;		//!< 1d

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


vec3 getCellSize0(in BrickParam param)
{
	return param.m_cell_size.xyz;	
}
vec3 getCellSize1(in BrickParam param)
{
	return param.m_subcell_size.xyz;	
}
uvec3 getIndexBrick0(in BrickParam param, in vec3 pos)
{
	vec3 cell = getCellSize0(param);
	return uvec3((pos - param.m_area_min.xyz) / cell);
}
uvec3 getIndexBrick1(in BrickParam param, in vec3 pos)
{
	vec3 cell = getCellSize1(param);
	return uvec3((pos - param.m_area_min.xyz) / cell);
}
uint getTiledIndexBrick1(in BrickParam param, in uvec3 index1)
{
//	return convert3DTo1D(index1, param.m_total_num.xyz);
	uvec3 index0 = uvec3(index1 / param.m_subcell_num.xyz);
	uvec3 index01 = uvec3(index1 - index0*param.m_subcell_num.xyz);
	uint z = convert3DTo1D(index01, param.m_subcell_num.xyz);
	return convert3DTo1D(index0, param.m_cell_num.xyz)*param.m_subcell_num.w + z;
}

bool isInRange(in uvec3 index, in uvec3 size)
{
	return all(lessThan(index, size));
}

