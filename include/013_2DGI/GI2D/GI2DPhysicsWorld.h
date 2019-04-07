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
		uint solver_count;
		float inertia;
		float mass;

		vec2 center;
		vec2 size;

		vec2 pos;
		vec2 pos_old;

		float angle;
		float angle_old;
		ivec2 exclusion;

		int exclusion_angle;
		int is_exclusive;
		vec2 pos_predict;

		float angle_predict;
		float _p1;
		vec2 cm;

		mat2 Aqq;
		mat2 Apq;
		mat2 Aqq_inv;

		mat2 R;
		mat2 S;

		ivec2 cm_work;
		ivec2 _pp;

		ivec4 Apq_work;

	};

	struct rbParticle
	{
		vec2 relative_pos;
		vec2 sdf;

		vec2 pos;
		vec2 pos_predict;

		vec2 local_pos;
		vec2 local_sdf;

		uint contact_index;
		uint is_contact;
		uint r_id;
		uint is_active;

		uint f_id;
		uint _p1;
		vec2 pos_old;
	};


	struct rbFluid
	{
		uint r_id;
		uint p_id;
		vec2 pos;

		vec2 local_pos;
		vec2 sdf;

		float mass;
		uint _p1;
		uint _p2;
		uint _p3;

	};

	struct rbConstraint
	{
		uint f_id1;
		uint f_id2;
	};

	enum
	{
		RB_NUM = 1024,
		RB_PARTICLE_BLOCK_SIZE = 64,
		RB_PARTICLE_NUM = RB_NUM * RB_PARTICLE_BLOCK_SIZE * 16,

	};

	PhysicsWorld(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context);
	void make(vk::CommandBuffer cmd, const uvec4& box);
	void execute(vk::CommandBuffer cmd);
	void executeMakeFluid(vk::CommandBuffer cmd);
	void executeMakeFluidWall(vk::CommandBuffer cmd);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	vk::UniqueDescriptorSetLayout m_physics_world_desc_layout;
	vk::UniqueDescriptorSet m_physics_world_desc;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<World> b_world;
	btr::BufferMemoryEx<Rigidbody> b_rigidbody;
	btr::BufferMemoryEx<rbParticle> b_rbparticle;
	btr::BufferMemoryEx<uint32_t> b_rbparticle_map;
	btr::BufferMemoryEx<uint32_t> b_fluid_counter;
	btr::BufferMemoryEx<rbFluid> b_fluid;
	btr::BufferMemoryEx<uvec4> b_constraint_counter;
	btr::BufferMemoryEx<rbConstraint> b_constraint;
	btr::BufferMemoryEx<vec2> b_centermass;

	uint32_t m_rigidbody_id;
	uint32_t m_particle_id;

};

