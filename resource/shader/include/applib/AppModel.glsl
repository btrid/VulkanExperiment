#ifndef APP_MODEL_GLSL
#define APP_MODEL_GLSL


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


#ifdef USE_AppModel
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
layout (set=USE_AppModel_Render, binding=10) uniform sampler2D t_base[16];
layout (set=USE_AppModel_Render, binding=11) uniform sampler2D t_normalcamera[16];
layout (set=USE_AppModel_Render, binding=12) uniform sampler2D t_emissive[16];
layout (set=USE_AppModel_Render, binding=13) uniform sampler2D t_metalness[16];
layout (set=USE_AppModel_Render, binding=14) uniform sampler2D t_roughness[16];
layout (set=USE_AppModel_Render, binding=15) uniform sampler2D t_occlusion[16];

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
	int	  isPlay;
	int   isLoop;
};
layout (set=USE_AppModel_Update, binding = 0) uniform sampler1DArray t_motion_texture[16];

layout(std430, set=USE_AppModel_Update, binding=1)restrict buffer AnimationInfoBuffer 
{
	AnimationInfo b_animation_info[];
};
layout(std430, set=USE_AppModel_Update, binding=2)restrict buffer PlayingAnimationBuffer 
{
	PlayingAnimation b_playing_animation[];
};
layout(std430, set=USE_AppModel_Update, binding=3) readonly restrict buffer NodeInfoBuffer {
	NodeInfo b_node_info[];
};
layout(std430, set=USE_AppModel_Update, binding=4) readonly restrict buffer BoneInfoBuffer {
	BoneInfo b_bone_info[];
};

layout(std430, set=USE_AppModel_Update, binding=5) restrict buffer NodeLocalTransformBuffer {
	mat4 b_node_transform[];
};
layout(std430, set=USE_AppModel_Update, binding=6) restrict buffer WorldTransform {
	mat4 b_world[];
};

layout(std430, set=USE_AppModel_Update, binding=7) restrict buffer BoneMapBuffer {
	uint b_instance_map[];
};

layout(std430, set=USE_AppModel_Update, binding=8) restrict buffer MeshBuffer {
	Mesh b_mesh[];
};


#endif


#endif