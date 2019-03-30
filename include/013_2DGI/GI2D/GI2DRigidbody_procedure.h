#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>


struct PhysicsWorld;
struct GI2DRigidbody;

struct GI2DRigidbody_procedure
{

	enum Shader
	{
		Shader_MakeParticle,
		Shader_MakeFluid,

		Shader_CalcForce,
		Shader_IntegrateParticle,
		Shader_Integrate,
		Shader_UpdateRigidbody,

		Shader_ToFragment,

		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Rigid,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MakeParticle,
		Pipeline_MakeFluid,

		Pipeline_CalcForce,
		Pipeline_IntegrateParticle,
		Pipeline_Integrate,
		Pipeline_UpdateRigidbody,

		Pipeline_ToFragment,

		Pipeline_Num,
	};

	GI2DRigidbody_procedure(const std::shared_ptr<PhysicsWorld>& world);
	void execute(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world);
	void executeMakeParticle(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world);
	void executeToFragment(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world);

	std::shared_ptr<PhysicsWorld> m_world;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};
