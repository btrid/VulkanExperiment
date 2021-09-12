#ifndef MODEL_H_
#define MODEL_H_

struct Mesh 
{
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


struct ModelInfo
{
	uint nodeNum;
	uint boneNum;
	uint mesh_num;
	uint node_depth_max;

	vec4 AABB;
	mat4 invGlobalMatrix;
};

struct NodeInfo
{
	int nodeNo;
	int parent;
	int _p;
	int depth;

};

struct BoneInfo
{
	int nodeIndex;
	int _p1;
	int _p2;
	int _p3;

	mat4 boneOffset;
};

struct ModelInstancingInfo
{
	uint instanceMaxNum;
	uint instanceAliveNum;
	uint instanceNum;
};


#ifdef USE_Model
layout(std430, set=USE_AppModel, binding=0) restrict buffer ModelInfoBuffer 
{
	ModelInfo u_model_info;
};
layout(std430, set=USE_AppModel, binding=1) restrict buffer ModelInstancingInfoBuffer
{
	ModelInstancingInfo b_model_instancing_info;
};
layout(std430, set=USE_AppModel, binding=2) restrict buffer BoneTransformBuffer {
	mat4 b_bone_transform[];
};
#endif

#ifdef USE_AppModel_Render

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

layout(std430, set=USE_AppModel_Render, binding=0) restrict buffer MaterialIndexBuffer {
	uint b_material_index[];
};
layout(std430, set=USE_AppModel_Render, binding=1) restrict buffer MaterialBuffer {
	Material b_material[];
};
layout (set=USE_AppModel_Render, binding=2) uniform sampler2D t_albedo_texture[16];
#endif

#ifdef USE_AppModel_Update

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


#endif
#endif