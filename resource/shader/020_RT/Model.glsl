#ifndef MODEL_H_
#define MODEL_H_

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_nonuniform_qualifier : require

struct Entity
{
	uint64_t VertexAddress;
	uint64_t NormalAddress;

	uint64_t TexcoordAddress;
	uint64_t IndexAddress;

	uint64_t MaterialAddress;
	uint64_t Material_Index;

	uint PrimitiveNum;
	uint _p;
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

layout(set=USE_Model_Entity, binding=0, scalar) buffer ModelEntityBuffer { Entity b_model_entity[]; };
layout(set=USE_Model_Resource, binding=0, scalar) buffer EntityBuffer { Entity b_entity[]; };
layout(set=USE_Model_Resource, binding=1, buffer_reference, scalar) buffer Vertex {vec3 b_v[]; };
layout(set=USE_Model_Resource, binding=2, buffer_reference, scalar) buffer Index {uint16_t b_i[]; };
//layout(set=USE_Model_Resource, binding=2, buffer_reference, scalar) buffer Index {u16vec3 b_i[]; };
layout(set=USE_Model_Resource, binding=3, buffer_reference, scalar) buffer Texcoord {vec2 b_t[]; };
layout(set=USE_Model_Resource, binding=4, buffer_reference, scalar) buffer MaterialBuffer {Material m[]; };
layout(set=USE_Model_Resource, binding=10) uniform sampler2D t_ModelTexture[];


#endif