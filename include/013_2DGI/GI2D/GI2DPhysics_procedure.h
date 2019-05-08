#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>


struct GI2DPhysics;
struct GI2DRigidbody;

struct GI2DPhysics_procedure
{

	enum Shader
	{
		Shader_ToFragment,

		Shader_RBMakeParticle,
		Shader_RBMakeCollidable,
		Shader_RBCollisionDetective,
		Shader_RBCollisionDetective_Fluid,
		Shader_RBCalcCenterMass,
		Shader_RBMakeTransformMatrix,
		Shader_RBUpdateParticleBlock,
		Shader_RBUpdateRigidbody,

		Shader_MakeWallCollision,

		Shader_DrawVoronoi,

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
		Pipeline_RBMakeCollidable,
		Pipeline_RBCollisionDetective,
		Pipeline_RBCollisionDetective_Fluid,
		Pipeline_RBCalcCenterMass,
		Pipeline_RBMakeTransformMatrix,
		Pipeline_RBUpdateParticleBlock,
		Pipeline_RBUpdateRigidbody,

		Pipeline_MakeWallCollision,

		Pipeline_DrawVoronoi,

		Pipeline_Num,
	};

	GI2DPhysics_procedure(const std::shared_ptr<GI2DPhysics>& world, const std::shared_ptr<GI2DSDF>& sdf);
	void execute(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& world, const std::shared_ptr<GI2DSDF>& sdf);

	void _executeMakeCollidableParticle(vk::CommandBuffer &cmd, const std::shared_ptr<GI2DPhysics>& world);
	void _executeMakeCollidableWall(vk::CommandBuffer &cmd, const std::shared_ptr<GI2DPhysics>& world, const std::shared_ptr<GI2DSDF>& sdf);

	void executeToFragment(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& world);
	void executeDrawVoronoi(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& world);


	std::shared_ptr<GI2DPhysics> m_world;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};

