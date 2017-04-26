struct Material{
	vec4		Ambient;
	vec4		Diffuse;
	vec4		Specular;
	vec4		Emissive;
#if 0
	uint64_t	DiffuseTex;
	uint64_t	AmbientTex;
	uint64_t	NormalTex;
	uint64_t	HeightTex;
	uint64_t	SpecularTex;
#else
	uvec2	DiffuseTex;
	uvec2	AmbientTex;
	uvec2	SpecularTex;
	uvec2	_pp;
#endif
	float		Shininess;
	float		_p;
	float		_p2;
	float		_p3;
};

struct Mesh {
	uint  count;
	uint  instanceCount;
	uint  firstIndex;
	uint  baseVertex;

	uint  baseInstance;
	int numElement;
	int numVertex;
	int nodeIndex;

	vec4 AABB;
};

struct Node
{
	int nodeNo;
	int parent;
	int boneIndex;
/*	int meshIndex1;
	int meshIndex2;
	int meshIndex3;
	int child1;
	int child2;
	int child3;
	int child4;
	int child5;
	int child6;
*/
	int _p1;

};

struct NodeLocalTransform
{
	mat3x4 local;
};
struct NodeGlobalTransform
{
	mat3x4 global;
};
struct ModelInfo
{
	int instanceMaxNum;
	int instanceAliveNum;
	int instanceNum;
	int nodeNum;

	int boneNum;
	int meshNum;
	int _p1;
	int _p2;
	vec4 AABB;
	mat4 invGlobalMatrix;
};

struct BoneInfo
{
	int nodeIndex;
	int _p1;
	int _p2;
	int _p3;

	mat4 boneOffset;
};

struct BoneTransform {
	mat4 finalTransform;
};

struct AnimationInfo
{
	float maxTime;
	float ticksPerSecond;
	int numInfo;
	int offsetInfo;
};

struct PlayingAnimation
{
	int   playingAnimationNo;
	float time;
	int	  currentDataIndex;
	int   isLoop;
};


struct MotionWork
{
	int motionInfoIndex;
	int motionDataIndex;
	int _p1;
	int _p2;
};
