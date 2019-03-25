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
		Shader_CollisionDetective,
		Shader_CollisionDetectiveBefore,
		Shader_CalcForce,
		Shader_Integrate,
		Shader_IntegrateAfter,

		Shader_CalcConstraint,
		Shader_SolveConstraint,

		Shader_ToFragment,

		Shader_MakeParticle,
		Shader_MakeFluid,

		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Rigid,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_CollisionDetective,
		Pipeline_CollisionDetectiveBefore,
		Pipeline_CalcForce,
		Pipeline_Integrate,
		Pipeline_IntegrateAfter,

		Pipeline_CalcConstraint,
		Pipeline_SolveConstraint,

		Pipeline_ToFragment,

		Pipeline_MakeParticle,
		Pipeline_MakeFluid,

		Pipeline_Num,
	};

	GI2DRigidbody_procedure(const std::shared_ptr<PhysicsWorld>& world);
	void execute(vk::CommandBuffer cmd, const std::vector<const GI2DRigidbody*>& rb);
	void executeMakeParticle(vk::CommandBuffer cmd, const std::vector<const GI2DRigidbody*>& rb);
	void executeToFragment(vk::CommandBuffer cmd, const std::vector<const GI2DRigidbody*>& rb);

	std::shared_ptr<PhysicsWorld> m_world;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};
