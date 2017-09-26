struct Material{
	vec4		Ambient;
	vec4		Diffuse;
	vec4		Specular;
	vec4		Emissive;
	uvec2	DiffuseTex;
	uvec2	AmbientTex;
	uvec2	SpecularTex;
	uvec2	_pp;
	float		Shininess;
	float		_p;
	float		_p2;
	float		_p3;
};

struct RayTraceMesh {
	uint  count;
	uint  instanceCount;
	uint  firstIndex;
	uint  baseVertex;
	uint  baseInstance;
	int numElement;
	int numVertex;
	int mIsInCamera;
	vec4 AABB;
};

