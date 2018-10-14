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

struct AnimationInfo
{
	float maxTime;
	float ticksPerSecond;
	int numInfo;
	int offsetInfo;
};

struct AnimationWork
{
	int   playingAnimationNo;
	float time;
	int	  currentDataIndex;
	int   isLoop;
};

#ifdef USE_APPMODEL

// どこでも使う
layout(std430, set=USE_APPMODEL, binding=0) restrict buffer ModelInfoBuffer 
{
	ModelInfo b_model_info;
};
layout(std430, set=USE_APPMODEL, binding=1) restrict buffer ModelInstancingInfoBuffer
{
	ModelInstancingInfo b_model_instancing_info;
};
layout(std430, set=USE_APPMODEL, binding=2) restrict buffer BoneTransformBuffer {
	mat4 b_bone_transforms[];
};
#endif

#ifdef USE_APPMODEL_RENDER
// render系
layout(std430, set=USE_APPMODEL_RENDER, binding=10) restrict buffer MaterialIndexBuffer {
	uint b_material_index[];
};
layout(std430, set=USE_APPMODEL_RENDER, binding=11) restrict buffer MaterialBuffer {
	Material b_material[];
};
layout (set=USE_APPMODEL_RENDER, binding = 12) uniform sampler2D tDiffuse[16];
#endif

#ifdef USE_APPMODEL_MOTION
// motionの更新とか
layout (set = USE_APPMODEL_MOTION, binding = 20) uniform sampler1DArray tMotionData;
layout(std430, set=USE_APPMODEL_MOTION, binding=21)restrict buffer AnimationInfoBuffer 
{
	AnimationInfo b_animation_info[];
};
layout(std430, set=USE_APPMODEL_MOTION, binding=22)restrict buffer AnimationWorkBuffer 
{
	AnimationWork b_animation_work[];
};
layout(std430, set=USE_APPMODEL_MOTION, binding=23) readonly restrict buffer NodeInfoBuffer {
	NodeInfo b_node_info[];
};
layout(std430, set=USE_APPMODEL_MOTION, binding=24) readonly restrict buffer BoneInfoBuffer {
	BoneInfo b_bone_info[];
};

layout(std430, set=USE_APPMODEL_MOTION, binding=25) restrict buffer NodeLocalTransformBuffer {
	mat4 b_node_transforms[];
};
layout(std430, set=USE_APPMODEL_MOTION, binding=26) readonly restrict buffer WorldTransform {
	mat4 b_worlds[];
};

layout(std430, set=USE_APPMODEL_MOTION, binding=27) restrict buffer BoneMapBuffer {
	uint b_instance_map[];
};

layout(std430, set=USE_APPMODEL, binding=28) restrict buffer MeshBuffer {
	Mesh b_draw_indirect[];
};

#endif


#endif