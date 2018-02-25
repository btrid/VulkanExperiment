#ifndef BTRLIB_MODEL_GLSL
#define BTRLIB_MODEL_GLSL

struct Material
{
	vec4	Ambient;
	vec4	Diffuse;
	vec4	Specular;
	vec4	Emissive;

	uint	u_albedo_texture;
	uint	u_ambient_texture;
	uint	u_specular_texture;
	uint	u_emissive_texture;

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

struct ModelInfo
{
	uint nodeNum;
	uint boneNum;
	uint mesh_num;
	uint node_depth_max;

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

#ifdef USE_MODEL_INFO_SET
layout(std430, set=USE_MODEL_INFO_SET, binding=0) restrict buffer ModelInfoBuffer 
{
	ModelInfo u_model_info;
};
layout(std430, set=USE_MODEL_INFO_SET, binding=1) restrict buffer ModelInstancingInfoBuffer
{
	ModelInstancingInfo u_model_instancing_info;
};
layout(std430, set=USE_MODEL_INFO_SET, binding=2) restrict buffer BoneTransformBuffer {
	mat4 b_bone_transform[];
};
layout(std430, set=USE_MODEL_INFO_SET, binding=3) restrict buffer MaterialIndexBuffer {
	uint b_material_index[];
};
layout(std430, set=USE_MODEL_INFO_SET, binding=4) restrict buffer MaterialBuffer {
	Material b_material[];
};
layout (set=USE_MODEL_INFO_SET, binding = 5) uniform sampler2D tDiffuse[16];
#endif

#ifdef USE_ANIMATION_INFO_SET
layout (set = USE_ANIMATION_INFO_SET, binding = 32) uniform sampler1DArray tMotionData;

layout(std430, set=USE_ANIMATION_INFO_SET, binding=0) restrict buffer ModelInfoBuffer 
{
	ModelInfo u_model_info;
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=1) restrict buffer ModelInstancingInfoBuffer
{
	ModelInstancingInfo u_model_instancing_info;
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=2)restrict buffer AnimationInfoBuffer 
{
	AnimationInfo animInfo[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=3)restrict buffer PlayingAnimationBuffer 
{
	PlayingAnimation playingMotion[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=4) readonly restrict buffer NodeInfoBuffer {
	NodeInfo nodes[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=5) readonly restrict buffer BoneInfoBuffer {
	BoneInfo boneInfo[];
};

layout(std430, set=USE_ANIMATION_INFO_SET, binding=6) restrict buffer NodeLocalTransformBuffer {
	mat4 nodeTransforms[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=7) readonly restrict buffer WorldTransform {
	mat4 worlds[];
};

layout(std430, set=USE_ANIMATION_INFO_SET, binding=8) restrict buffer BoneMapBuffer {
	uint b_instance_map[];
};

layout(std430, set=USE_ANIMATION_INFO_SET, binding=9) restrict buffer MeshBuffer {
	Mesh meshs[];
};
layout(std430, set=USE_ANIMATION_INFO_SET, binding=10) restrict buffer BoneTransformBuffer {
	mat4 b_bone_transform[];
};

#endif


#endif