#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct GI2DRigidbody;

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
	};

	struct rbFluid
	{
		uint id;
		float mass;
		vec2 vel;
	};
	PhysicsWorld(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context);
	void execute(vk::CommandBuffer cmd);
	void executeMakeFluid(vk::CommandBuffer cmd, const GI2DRigidbody* rb);
	void executeMakeFluidWall(vk::CommandBuffer cmd);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	vk::UniqueDescriptorSetLayout m_rigitbody_desc_layout;

	vk::UniqueDescriptorSetLayout m_physics_world_desc_layout;
	vk::UniqueDescriptorSet m_physics_world_desc;

	vk::UniqueDescriptorSetLayout m_rigitbodys_desc_layout;
	vk::UniqueDescriptorSet m_rigitbodys_desc;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<World> b_world;
	btr::BufferMemoryEx<uint32_t> b_fluid_counter;
	btr::BufferMemoryEx<rbFluid> b_fluid;
};

