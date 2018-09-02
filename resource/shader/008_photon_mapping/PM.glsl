
#ifndef PM_GLSL_
#define PM_GLSL_

struct Vertex{
	vec3 Position;
	int MaterialIndex;
	vec2 m_texcoord;
	vec2 m_texcoord1;
};

struct TriangleMesh
{
	Vertex a, b, c;
};

struct Material
{
	vec4 	Diffuse;
	vec4 	Emissive;
	float	m_diffuse;
	float	m_specular; //!< 光沢
	float	m_rough; //!< ざらざら
	float	m_metalic;

	int		is_emissive;
//	uvec2	m_diffuse_tex;
//	uvec2	m_ambient_tex;
//	uvec2	m_specular_tex;
//	uvec2	m_emissive_tex;

};

struct DrawCommand// : public DrawElementsIndirectCommand
{
	uint count;
	uint instanceCount;
	uint firstIndex;
	uint baseVertex;
	
	uint baseInstance;
	uint is_emissive;
	uint _p[2];

	vec4 aabb_max;
	vec4 aabb_min;
};

struct BounceData{
	int count;
	int startOffset;
	int calced;
	int _p;
};
struct PMInfo
{
	uvec4 num0;
	vec4 cell_size;
	vec4 area_min;
	vec4 area_max;
};

#ifdef USE_PM
layout(set=USE_PM, binding=0, std140) uniform PMUniform {
	PMInfo u_pm_info;
};


layout(set=USE_PM, binding=1, std430) restrict buffer  CMDBuffer{
	DrawCommand b_cmd[];
};
layout(set=USE_PM, binding=2, std430) restrict buffer  VertexBuffer{
	Vertex b_vertex[];
};
layout(set=USE_PM, binding=3, std430) restrict buffer  ElementBuffer{
	ivec3 b_element[];
};
layout(set=USE_PM, binding=4, std430) restrict buffer MaterialBuffer {
	Material b_material[];
};
layout(set=USE_PM, binding=10, std430) restrict buffer TriangleCounter{
	uvec4 b_triangle_count[];
};
layout(set=USE_PM, binding=11, std430) restrict buffer TriangleLLHeadBuffer{
	uint bTriangleLLHead[];
};
layout(set=USE_PM, binding=12, std430) restrict buffer TriangleLLBuffer{
	TriangleLL bTriangleLL[];
};
layout(set=USE_PM, binding=20, std430) restrict buffer  PhotonBuffer{
	Photon bPhoton[];
};
layout(set=USE_PM, binding=21, std430) buffer  PhotonCounter{
	uvec4 b_photon_counter[];
}
layout(set=USE_PM, binding=22, std430) restrict buffer PhotonLLHeadBuffer{
	uint bPhotonLLHead[];
};
layout(set=USE_PM, binding=23, std430) restrict buffer PhotonLLBuffer{
	uint bPhotonLL[];
};
layout(set=USE_PM, binding=24, std430) restrict buffer PhotonBounceBuffer{
	BounceData bPhotonBounce[];
};

layout(set=USE_PM, binding=1, r8ui) uniform readonly uimage3D tBrickMap0;
layout(binding = 1, rgba32f) uniform image2D tRender;

#endif


struct MarchResult{
	Hit HitResult;
	TriangleMesh HitTriangle;
	uint TriangleLLIndex;
};

vec3 getDiffuse(in Material m, in MarchResult result)
{
	if(m.m_diffuse_tex == 0){
		return m.Diffuse.xyz;
	}

	vec2 texcoord 
		= result.HitTriangle.a.m_texcoord*result.HitResult.Weight.x
		+ result.HitTriangle.b.m_texcoord*result.HitResult.Weight.y
		+ result.HitTriangle.c.m_texcoord*result.HitResult.Weight.z;

	return texture(sampler2D(m.m_diffuse_tex), texcoord).xyz;
}

vec3 sampling(in Material material, in vec3 inDir, in MarchResult hit)
{
//	return material.Diffuse.xyz / 3.1415;
	return getDiffuse(material, hit) / 3.1415;
}
struct Photon
{
	vec3 Position;
	uint Dir_16_16; //!< 極座標[theta, phi]
	vec3 Power; //!< flux 放射束
//	uint Power10_11_11; //!< @todo
	uint TriangleLLIndex; //!< hitした三角形のインデックス
};

int compressDirection(in vec3 dir)
{
	float pi = 3.1415;
	float intmax = float(0xFFFF);
	int theta = int(acos(dir.z) / pi * intmax);
	int phi = int((dir.y < 0.f ? -1.f : 1.f) * acos(dir.x / sqrt(dir.x*dir.x + dir.y*dir.y)) / pi * intmax);
	return (theta & 0xffff) << 16 | ((phi & 0xffff));
}

vec3 decompressDirection(in int dir)
{
	uint theta = dir >> 16;
	uint phi = dir & 0xFFFF;

	float pi = 3.1415;
	float intmax = float(0xFFFF);
	return vec3(
		sin(theta*pi / intmax)*cos(phi*pi / intmax),
		sin(theta*pi / intmax)*sin(phi*pi / intmax),
		cos(theta*pi / intmax));
}

void setDirection(inout Photon p, in vec3 dir)
{
	float pi = 3.1415;
	float intmax = float(0xFFFF);
	int theta = int(acos(dir.z) / pi * intmax);
	int phi = int((dir.y < 0.f ? -1.f : 1.f) * acos(dir.x / sqrt(dir.x*dir.x + dir.y*dir.y)) / pi * intmax);
	p.Dir_16_16 = (theta & 0xffff) << 16 | ((phi & 0xffff));
}
vec3 getDirection(in Photon p){
	uint theta = p.Dir_16_16 >> 16;
	uint phi = p.Dir_16_16 & 0xFFFF;

	float pi = 3.1415;
	float intmax = float(0xFFFF);
	return vec3(
		sin(theta*pi / intmax)*cos(phi*pi / intmax),
		sin(theta*pi / intmax)*sin(phi*pi / intmax),
		cos(theta*pi / intmax));
}

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

vec3 calcDir(in vec3 n, in float v)
{
	// 逆関数法で反射方向を決める
	float r1 = rand(vec2(v*1.4, v*2.1));
	float phi = r1 * 2. * 3.1415;

	float r2 = rand(vec2(v*2.4, v*4.3));
	float r2sq = sqrt(r2);

	float tx = r2sq * cos(phi);
	float ty = r2sq * sin(phi);
	float tz = sqrt(1. - r2);

	vec3 t = abs(n.x) > abs(n.y) 
		? normalize(cross(vec3(0., 1., 0.), n))
		: normalize(cross(vec3(1., 0., 0.), n));
	vec3 b = normalize(cross(n, t));
	vec3 dir = tz * n + tx * t + ty * b;
	dir = normalize(dir);

	return dir;
}

#endif // PM_GLSL_
