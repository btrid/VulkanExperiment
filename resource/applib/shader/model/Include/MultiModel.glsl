struct Plane
{
	vec3 normal;
	float n;
};
struct Frustom{
	Plane p[6];
};

struct Camera2
{
	mat4 uProjection;
	mat4 uView;
	Frustom uFrustom;
};
struct Material
{
	vec4		Ambient;
	vec4		Diffuse;
	vec4		Specular;
	vec4		Emissive;
	float		Shininess;
	float		_p;
	float		_p2;
	float		_p3;
};

struct Mesh {
	uint	count;
	uint	instanceCount;
	uint	firstIndex;
	uint	baseVertex;

	uint	baseInstance;
	uint	m_vertex_num;
	uint	m_node_index;	//!< 
	uint	m_material_index;

	vec4 AABB;
};

struct NodeInfo
{
	int nodeNo;
	int parent;
	int _p;
	int depth;

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
	int nodeNum;
	int boneNum;
	int mesh_num;
	int node_depth_max;

	vec4 AABB;
	mat4 invGlobalMatrix;
};

struct ModelInstancingInfo
{
	uint instanceMaxNum;
	uint instanceAliveNum;
	uint instanceNum;
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

#ifdef USE_ANIMATION_INFO_SET
layout (set = USE_ANIMATION_INFO_SET, binding = 32) uniform sampler1DArray tMotionData;

layout(std430, set=USE_ANIMATION_INFO_SET, binding=0)restrict buffer AnimationInfoBuffer 
{
	AnimationInfo animInfo[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=1)restrict buffer PlayingAnimationBuffer 
{
	PlayingAnimation playingMotion[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=2) readonly restrict buffer NodeInfoBuffer {
	NodeInfo nodes[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=3) readonly restrict buffer BoneInfoBuffer {
	BoneInfo boneInfo[];
};

layout(std430, set=USE_ANIMATION_INFO_SET, binding=4) restrict buffer NodeLocalTransformBuffer {
	mat4 nodeLocalTransforms[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=5) restrict buffer NodeGlobalTransformBuffer {
	mat4 nodeGlobalTransforms[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=6) readonly restrict buffer WorldTransform {
	mat4 worlds[];
};

layout(std430, set=USE_ANIMATION_INFO_SET, binding=7) restrict buffer BoneMapBuffer {
	uint boneMap[];
};

layout(std430, set=USE_ANIMATION_INFO_SET, binding=8) restrict coherent buffer MeshBuffer {
	Mesh meshs[];
};
#endif

