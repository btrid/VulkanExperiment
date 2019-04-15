#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>
#include <013_2DGI/GI2D/GI2DRigidbody.h>

struct PhysicsWorld
{
	enum Shader
	{
		Shader_ToFluid,
		Shader_ToFluidWall,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_ToFluid,
		PipelineLayout_ToFluidWall,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_ToFluid,
		Pipeline_ToFluidWall,
		Pipeline_Num,
	};

	enum DescriptorLayout
	{
		DescLayout_Data,
		DescLayout_Make,
		DescLayout_Num,
	};
	struct World
	{
		float DT;
		uint step;
		uint STEP;
		uint rigidbody_num;
//		uint particle_block_num;
	};
	struct Rigidbody
	{
		uint pnum;
		uint _p;
		vec2 cm;

		vec4 R;

		i64vec2 cm_work;
		i64vec2 _pp;
		i64vec4 Apq_work;
	};

	struct rbParticle
	{
		vec2 relative_pos;
		vec2 sdf;

		vec2 pos;
		vec2 pos_old;

		vec2 local_pos;
		vec2 local_sdf;

		uint contact_index;
		uint is_contact;
		uint color;
		uint is_active;
	};

	struct rbFluid
	{
		uint r_id;
		float mass;
		vec2 pos;

		vec2 vel;
		vec2 sdf;
	};

// 	struct BufferManage
// 	{
// 		uint rb_active_index;
// 		uint rb_free_index;
// 		uint particle_active_index;
// 		uint particle_free_index;
// 	};

	enum
	{
		RB_NUM = 512,
		RB_PARTICLE_BLOCK_SIZE = 64,
		RB_PARTICLE_NUM = RB_NUM * RB_PARTICLE_BLOCK_SIZE * 8,

		MAKE_RB_SIZE_MAX = 128,
		MAKE_RB_BIT_SIZE = MAKE_RB_SIZE_MAX/8,
	};

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorLayout i)const { return m_desc_layout[i].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorLayout i)const { return m_descset[i].get(); }

	PhysicsWorld(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context);
	void make(vk::CommandBuffer cmd, const uvec4& box);
	void execute(vk::CommandBuffer cmd);
	void executeMakeFluid(vk::CommandBuffer cmd);
	void executeMakeFluidWall(vk::CommandBuffer cmd);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueDescriptorSetLayout, DescLayout_Num> m_desc_layout;
	std::array<vk::UniqueDescriptorSet, DescLayout_Num> m_descset;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<World> b_world;
	btr::BufferMemoryEx<Rigidbody> b_rigidbody;
	btr::BufferMemoryEx<rbParticle> b_rbparticle;
	btr::BufferMemoryEx<uint32_t> b_rbparticle_map;
	btr::BufferMemoryEx<uint32_t> b_fluid_counter;
	btr::BufferMemoryEx<rbFluid> b_fluid;

	btr::BufferMemoryEx<uvec4> b_manager;
	btr::BufferMemoryEx<uint> b_rb_freelist;
	btr::BufferMemoryEx<uint> b_particle_freelist;

	btr::BufferMemoryEx<rbParticle> b_make_particle;
	btr::BufferMemoryEx<uint64_t> b_posbit;
	btr::BufferMemoryEx<u16vec2> b_jfa_cell;

	uint32_t m_rigidbody_id;
	uint32_t m_particle_id;

};

