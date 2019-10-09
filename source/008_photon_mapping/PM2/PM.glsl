

struct Vertex{
	vec3 Position;
	int MaterialIndex;
};

struct TriangleMesh
{
	Vertex a, b, c;
};

struct Material
{
	vec4 Diffuse;
	vec4 Emissive;
	float m_diffuse;
	float m_specular; //!< 光沢
	float m_rough; //!< ざらざら
	float m_metalic;

};

struct Photon
{
	vec3 Position;
	uint Dir_16_16; //!< 極座標[theta, phi]
	vec3 Power; //!< flux 放射束
	uint Power10_11_11; //!< @todo
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
	float intmax = float(32767);
	int theta = int(acos(dir.z) / pi * intmax);
	int phi = int((dir.y < 0.f ? -1.f : 1.f) * acos(dir.x / sqrt(dir.x*dir.x + dir.y*dir.y)) / pi * intmax);
	p.Dir_16_16 = ((theta & 0xffff) << 16) | ((phi & 0xffff));
//	p.Power = dir;
}
vec3 getDirection(in Photon p)
{
//		return p.Power;

	uint theta = p.Dir_16_16 >> 16;
	uint phi = p.Dir_16_16 & 0xFFFF;

	float pi = 3.1415;
	float intmax = float(32767);
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

struct MarchResult{
	Hit HitResult;
	TriangleMesh HitTriangle;
};
