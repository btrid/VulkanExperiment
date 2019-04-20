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
		Shader_ToFragment,

		Shader_RBMakeParticle,
		Shader_RBMakeFluid,
		Shader_RBConstraintSolve,
		Shader_RBCalcCenterMass,
		Shader_RBMakeTransformMatrix,
		Shader_RBUpdateParticleBlock,
		Shader_RBUpdateRigidbody,

		Shader_MakeWallCollision,

		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Rigid,
		PipelineLayout_MakeWallCollision,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_ToFragment,

		Pipeline_RBMakeParticle,
		Pipeline_RBMakeFluid,
		Pipeline_RBConstraintSolve,
		Pipeline_RBCalcCenterMass,
		Pipeline_RBMakeTransformMatrix,
		Pipeline_RBUpdateParticleBlock,
		Pipeline_RBUpdateRigidbody,

		Pipeline_MakeWallCollision,

		Pipeline_Num,
	};

	GI2DRigidbody_procedure(const std::shared_ptr<PhysicsWorld>& world, const std::shared_ptr<GI2DSDF>& sdf);
	void execute(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world, const std::shared_ptr<GI2DSDF>& sdf);
	void executeMakeFluid(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world);

	void _executeMakeFluidParticle(vk::CommandBuffer &cmd, const std::shared_ptr<PhysicsWorld>& world);
	void _executeMakeFluidWall(vk::CommandBuffer &cmd, const std::shared_ptr<PhysicsWorld>& world, const std::shared_ptr<GI2DSDF>& sdf);

	void executeToFragment(vk::CommandBuffer cmd, const std::shared_ptr<PhysicsWorld>& world);


	std::shared_ptr<PhysicsWorld> m_world;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};

