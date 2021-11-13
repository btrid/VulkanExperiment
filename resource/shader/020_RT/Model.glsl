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

struct Entity
{
	uint64_t VertexAddress;
	uint64_t NormalAddress;

	uint64_t TexcoordAddress;
	uint64_t IndexAddress;

	uint64_t MaterialAddress;
	uint64_t Material_Index;

	uint64_t MeshAddress;
	uint64_t _unuse;

	uint64_t MeshletDesc;
	uint64_t MeshletPack;

	uint PrimitiveNum;
	uint _p;
	uint _p2;
	uint _p3;
};
struct Mesh
{
	vec3 bboxMin;
	uint m_vertex_num;
	vec3 bboxMax;
	uint m_primitive_num;
};

struct Material
{
	vec4 m_basecolor_factor;
	float m_metallic_factor;
	float m_roughness_factor;
	float _p1;
	float _p2;
	vec3  m_emissive_factor;
	float _p11;

	uint TexID_Base;
	uint TexID_MR;
	uint TexID_AO;
	uint TexID_Normal;

	uint TexID_Emissive;
	uint Tex_Base_;
	uint Tex_Bas_e;
	uint Tex_Ba_se;
};

layout(set=USE_Model_Resource, binding=0, scalar) buffer EntityBuffer { Entity b_model_entity[]; };
layout(set=USE_Model_Resource, binding=1, buffer_reference, scalar) buffer Vertex {vec3 b_v[]; };
layout(set=USE_Model_Resource, binding=2, buffer_reference, scalar) buffer Index {uint16_t b_i[]; };
layout(set=USE_Model_Resource, binding=3, buffer_reference, scalar) buffer Texcoord {vec2 b_t[]; };
layout(set=USE_Model_Resource, binding=4, buffer_reference, scalar) buffer MaterialBuffer {Material m[]; };
layout(set=USE_Model_Resource, binding=5, buffer_reference, scalar) buffer MeshBuffer {Mesh b_mesh[]; };
layout(set=USE_Model_Resource, binding=6, buffer_reference, scalar) buffer BindlessBufferU32 {
	uint b_uint[];
};
layout(set=USE_Model_Resource, binding=7, buffer_reference, scalar) buffer BindlessBufferU32x2 {
	uvec2 b_uvec2[];
};
layout(set=USE_Model_Resource, binding=8, buffer_reference, scalar) buffer BindlessBufferU32x3 {
	uvec3 b_uvec3[];
};
layout(set=USE_Model_Resource, binding=9, buffer_reference, scalar) buffer BindlessBufferU32x4 {
	uvec4 b_uvec4[];
};

layout(set=USE_Model_Resource, binding=10) uniform sampler2D t_ModelTexture[];

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

struct VkDrawMeshTasksIndirectCommand
{
    uint    taskCount;
    uint    firstTask;
};
struct IndirectCmd
{
	VkDrawMeshTasksIndirectCommand task;
	int InstanceIndex;
};
// must match cadscene!

struct ObjectData 
{
  mat4 worldMatrix;
  mat4 worldMatrixIT;
  int modelID;
};


layout(set=USE_Model_Render, binding=0, /*buffer_reference,*/ scalar) buffer IndirectCmdBuffer { IndirectCmd b_cmd[]; };
//layout(set=USE_Model_Render, binding=1, /*buffer_reference,*/ scalar) buffer ASInstanceBuffer { VkAccelerationStructureInstance b_AS_instance[]; };
layout(set=USE_Model_Render, binding=1, /*buffer_reference,*/ scalar) buffer ObjectDataBuffer { ObjectData b_object[]; };

#endif // USE_Model_Render
#endif

