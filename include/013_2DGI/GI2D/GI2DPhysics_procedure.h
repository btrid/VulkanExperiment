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
		Shader_DrawParticle,

		Shader_RBMakeParticle,
		Shader_MakeCollision,
		Shader_MakeWallCollision,
		Shader_RBCollisionDetective,
		Shader_RBCalcPressure,
		Shader_RBCalcCenterMass,
		Shader_RBMakeTransformMatrix,
		Shader_RBUpdateParticleBlock,
		Shader_RBUpdateRigidbody,

		ShaderDebug_DrawCollisionHeatMap,

		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Rigid,
		PipelineLayout_DrawParticle,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_DrawParticle,

		Pipeline_RBMakeParticle,
		Pipeline_MakeCollision,
		Pipeline_MakeWallCollision,

		Pipeline_RBCollisionDetective,
		Pipeline_RBCalcPressure,
		Pipeline_RBCalcCenterMass,
		Pipeline_RBMakeTransformMatrix,
		Pipeline_RBUpdateParticleBlock,
		Pipeline_RBUpdateRigidbody,

		Pipeline_DrawCollisionHeatMap,

		Pipeline_Num,
	};

	GI2DPhysics_procedure(const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<GI2DSDF>& sdf);
	void execute(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<GI2DSDF>& sdf);
	void executeDrawParticle(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<RenderTarget>& render_target);
	void executeDebugDrawCollisionHeatMap(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<RenderTarget>& render_target);

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};


namespace gi2d_physics
{
	
int rigidbody();


}