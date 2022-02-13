#ifndef MODEL_H_
#define MODEL_H_

#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_NV_mesh_shader : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require

struct VkDrawMeshTasksIndirectCommand
{
    uint    taskCount;
    uint    firstTask;
};

struct Mesh
{
	uint64_t PrimitiveAddress;
};

struct Primitive
{
	VkDrawMeshTasksIndirectCommand task;
	uint64_t PrimitiveAddress; // 自分の位置

	uint64_t VertexAddress;
	uint64_t IndexAddress;

	uint64_t NormalAddress;
	uint64_t TexcoordAddress;

	uint64_t TangentAddress;
	uint64_t MaterialAddress;

	uint64_t MeshletDesc;
	uint64_t MeshletPack;

	vec3 bboxMin;
	uint m_unuse;
	vec3 bboxMax;
	uint m_primitive_num_unuse;
};

struct Material
{
	vec4 m_basecolor_factor;

	float m_metallic_factor;
	float m_roughness_factor;
	bool m_is_emissive;
	float _p2;

	vec3  m_emissive_factor;
	float _p11;

	int TexID_Base;
	int TexID_MR;
	int TexID_AO;
	int TexID_Normal;

	int TexID_Emissive;
	int Tex_Base_;
	int Tex_Bas_e;
	int Tex_Ba_se;
};

layout(set=USE_Model_Resource, binding=0, scalar) buffer ModelBuffer { Mesh b_mesh[];};
layout(set=USE_Model_Resource, binding=1, buffer_reference, scalar) buffer PrimitiveBuffer {Primitive b_primitive[]; };
layout(set=USE_Model_Resource, binding=2, buffer_reference, scalar) buffer Vertex {vec3 b_v[]; };
layout(set=USE_Model_Resource, binding=3, buffer_reference, scalar) buffer Index {uint16_t b_i[]; };
layout(set=USE_Model_Resource, binding=4, buffer_reference, scalar) buffer Texcoord {vec2 b_t[]; };
layout(set=USE_Model_Resource, binding=5, buffer_reference, scalar) buffer MaterialBuffer {Material m[]; };
layout(set=USE_Model_Resource, binding=6, buffer_reference, scalar) buffer BindlessBufferU32 {uint b_uint[];};
layout(set=USE_Model_Resource, binding=7, buffer_reference, scalar) buffer BindlessBufferU32x2 {uvec2 b_uvec2[];};
layout(set=USE_Model_Resource, binding=8, buffer_reference, scalar) buffer BindlessBufferU32x3 {uvec3 b_uvec3[];};
layout(set=USE_Model_Resource, binding=9, buffer_reference, scalar) buffer BindlessBufferU32x4 {uvec4 b_uvec4[];};

layout(set=USE_Model_Resource, binding=100) uniform sampler2D t_ModelTexture[];

#ifdef USE_Model_Render
struct VkAccelerationStructureInstance 
{
    mat3x4 transform;
//  uint32_t instanceCustomIndex:24;
//  uint32_t mask:8;
	uint index_mask;
//  uint32_t instanceShaderBindingTableRecordOffset:24;
//  VkGeometryInstanceFlagsKHR flags:8;
	uint _d;
    uint64_t accelerationStructureReference;
} ;
uint instanceCustomIndex(VkAccelerationStructureInstance instance){ return instance.index_mask&((1<<24)-1);}

struct IndirectCmd
{
	VkDrawMeshTasksIndirectCommand task;
	uint64_t PrimitiveAddress;
};
// must match cadscene!

struct ObjectData 
{
  mat4 worldMatrix;
  mat4 worldMatrixIT;
};


layout(set=USE_Model_Render, binding=0, /*buffer_reference,*/ scalar) buffer IndirectCmdBuffer { IndirectCmd b_cmd[]; };
layout(set=USE_Model_Render, binding=1, /*buffer_reference,*/ scalar) buffer ObjectDataBuffer { ObjectData b_object[]; };

#endif // USE_Model_Render
#endif

