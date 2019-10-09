
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
	uvec2	m_diffuse_tex;
	uvec2	m_ambient_tex;
	uvec2	m_specular_tex;
	uvec2	m_emissive_tex;

};

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
