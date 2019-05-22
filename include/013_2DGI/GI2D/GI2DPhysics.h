#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>
#include <013_2DGI/GI2D/GI2DRigidbody.h>

struct GI2DPhysics
{
#define COLLIDABLE_NUM (4)
	enum Shader
	{
		Shader_ToFluid,
		Shader_ToFluidWall,

		Shader_MakeRB_Register,
		Shader_MakeRB_MakeJFCell,
		Shader_MakeRB_MakeSDF,

		Shader_Voronoi_SetupJFA,
		Shader_Voronoi_MakeJFA,
		Shader_Voronoi_MakeTriangle,
		Shader_Voronoi_MakeTriangle2,
		Shader_Voronoi_SortTriangleVertex,

		Shader_Voronoi_MakePath,

		Shader_MakeRB_DestructWall_VS,
		Shader_MakeRB_DestructWall_GS,
		Shader_MakeRB_DestructWall_FS,

		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_ToFluid,
		PipelineLayout_ToFluidWall,

		PipelineLayout_MakeRB,
		PipelineLayout_DestructWall,

		PipelineLayout_Voronoi,

		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_ToFluid,
		Pipeline_ToFluidWall,

		Pipeline_MakeRB_Register,
		Pipeline_MakeRB_MakeJFCell,
		Pipeline_MakeRB_MakeSDF,

		Pipeline_Voronoi_SetupJFA,
		Pipeline_Voronoi_MakeJFA,
		Pipeline_Voronoi_MakeTriangle,
		Pipeline_Voronoi_MakeTriangle2,
		Pipeline_Voronoi_SortTriangleVertex,
		Pipeline_Voronoi_MakePath,

		Pipeline_MakeRB_DestructWall,
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

		uint rigidbody_max;
		uint particleblock_max;
		uint gpu_index;
		uint cpu_index;
	};
#define RB_FLAG_FLUID (1)
	struct Rigidbody
	{
		uint pnum;
		float life;
		vec2 cm;

		uint flag;
		uint _p1;
		i16vec2 size_min;
		i16vec2 size_max;

		vec4 R;

		i64vec2 cm_work;
		i64vec4 Apq_work;
	};

#define RBP_FLAG_ACTIVE (1)
#define RBP_FLAG_COLLIDABLE (2)
	struct rbParticle
	{
		vec2 relative_pos;
		vec2 sdf;

		vec2 pos;
		vec2 pos_old;

		vec2 local_pos;
		vec2 local_sdf;

		uint contact_index;
		float density;
		uint color;
		uint flag;
	};

	struct rbCollidable
	{
		uint r_id;
		float mass;
		vec2 pos;

		vec2 vel;
		vec2 sdf;
	};

	struct BufferManage
	{
		uint rb_list_size;
		uint pb_list_size;
		uint rb_active_index;
		uint rb_free_index;
		uint particle_active_index;
		uint particle_free_index;
	};

	struct VoronoiCell
	{
		i16vec2 point;
	};

#define VoronoiVertex_MAX (10)
	struct VoronoiPolygon
	{
		int16_t vertex_index[VoronoiVertex_MAX];
		int num;
		i16vec4 minmax;
	};

	struct VoronoiVertex
	{
		i16vec2 point;
		int16_t	cell[4];
	};

	struct RBMakeParam
	{
		uvec4 pb_num;
		uvec4 registered_num;
		ivec2 rb_size;
	};
	enum
	{
		RB_NUM_MAX = 1024*4,
		RB_PARTICLE_BLOCK_NUM_MAX = RB_NUM_MAX * 8,
		RB_PARTICLE_BLOCK_SIZE = 64, // shaderも64前提の部分がある
		RB_PARTICLE_NUM = RB_PARTICLE_BLOCK_NUM_MAX * RB_PARTICLE_BLOCK_SIZE,

		MAKE_RB_SIZE_MAX = RB_PARTICLE_BLOCK_SIZE * 16*16,
		MAKE_RB_JFA_CELL = 128*128,
	};

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorLayout i)const { return m_desc_layout[i].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorLayout i)const { return m_descset[i].get(); }

	GI2DPhysics(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context);
	void make(vk::CommandBuffer cmd, const uvec4& box);
	void execute(vk::CommandBuffer cmd);
	void executeDestructWall(vk::CommandBuffer cmd);
	void executeMakeFluidWall(vk::CommandBuffer cmd);
	void executeMakeVoronoi(vk::CommandBuffer cmd);
	void executeMakeVoronoiPath(vk::CommandBuffer cmd);

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueDescriptorSetLayout, DescLayout_Num> m_desc_layout;
	std::array<vk::UniqueDescriptorSet, DescLayout_Num> m_descset;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	btr::BufferMemoryEx<World> b_world;
	btr::BufferMemoryEx<Rigidbody> b_rigidbody;
	btr::BufferMemoryEx<rbParticle> b_rbparticle;
	btr::BufferMemoryEx<uint32_t> b_rbparticle_map;
	btr::BufferMemoryEx<uint32_t> b_collidable_counter;
	btr::BufferMemoryEx<rbCollidable> b_collidable;
	btr::BufferMemoryEx<uint> b_fluid_counter;

	btr::BufferMemoryEx<BufferManage> b_manager;
	btr::BufferMemoryEx<uint> b_rb_memory_list;
	btr::BufferMemoryEx<uint> b_pb_memory_list;
	btr::BufferMemoryEx<uvec4> b_update_counter;
	btr::BufferMemoryEx<uint> b_rb_update_list;
	btr::BufferMemoryEx<uint> b_pb_update_list;
	btr::BufferMemoryEx<VoronoiCell> b_voronoi_cell;
	btr::BufferMemoryEx<VoronoiPolygon> b_voronoi_polygon;
	btr::BufferMemoryEx<int16_t> b_voronoi;
	btr::BufferMemoryEx<uvec4> b_voronoi_vertex_counter;
	btr::BufferMemoryEx<VoronoiVertex> b_voronoi_vertex;
	btr::BufferMemoryEx<int16_t> b_voronoi_path;


	btr::BufferMemoryEx<Rigidbody> b_make_rigidbody;
	btr::BufferMemoryEx<rbParticle> b_make_particle;
	btr::BufferMemoryEx<i16vec2> b_make_jfa_cell;
	btr::BufferMemoryEx<RBMakeParam> b_make_param;

	World m_world;

};


struct GI2DPhysicsDebug
{
	GI2DPhysicsDebug(const std::shared_ptr<GI2DPhysics>& physics_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = physics_context;
		m_render_target = render_target;

		auto& context = m_context->m_context;
		{
			const char* name[] =
			{
				"Debug_DrawVoronoiTriangle.vert.spv",
				"Debug_DrawVoronoiTriangle.geom.spv",
				"Debug_DrawVoronoiTriangle.frag.spv",

				"Debug_DrawVoronoiPath.geom.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		{
			vk::DescriptorSetLayout layouts[] = {
				m_context->getDescriptorSetLayout(GI2DPhysics::DescLayout_Data),
			};
			vk::PushConstantRange ranges[] = {
				vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eGeometry),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(std::size(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(std::size(ranges));
			pipeline_layout_info.setPPushConstantRanges(ranges);
			m_pipeline_layout[PipelineLayout_DrawVoronoiTriangle] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}


		// レンダーパス
		{
			// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(std::size(color_ref));
			subpass.setPColorAttachments(color_ref);

			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(render_target->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(std::size(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}

		{
			vk::ImageView view[] = {
				render_target->m_view,
			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(std::size(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(render_target->m_info.extent.width);
			framebuffer_info.setHeight(render_target->m_info.extent.height);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
		}

		// pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::ePointList);

			vk::Extent3D size = render_target->m_info.extent;
			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height));

			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(1);
			viewport_info.setPViewports(&viewport);
			viewport_info.setScissorCount(1);
			viewport_info.setPScissors(&scissor);

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eBack);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setLineWidth(2.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			// ブレンド
			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			// vertexinput
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineShaderStageCreateInfo drawtriangle_shader[] =
			{
				vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, m_shader[Shader_DrawVoronoiTriangle_VS].get(), "main"),
				vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eGeometry, m_shader[Shader_DrawVoronoiTriangle_GS].get(), "main"),
				vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, m_shader[Shader_DrawVoronoiTriangle_FS].get(), "main"),
			};
			vk::PipelineShaderStageCreateInfo drawpath_shader[] =
			{
				vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, m_shader[Shader_DrawVoronoiTriangle_VS].get(), "main"),
				vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eGeometry, m_shader[Shader_DrawVoronoiPath_GS].get(), "main"),
				vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, m_shader[Shader_DrawVoronoiTriangle_FS].get(), "main"),
			};
			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(std::size(drawtriangle_shader))
				.setPStages(drawtriangle_shader)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayout_DrawVoronoiTriangle].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),

				vk::GraphicsPipelineCreateInfo()
				.setStageCount(std::size(drawpath_shader))
				.setPStages(drawpath_shader)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayout_DrawVoronoiTriangle].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[Pipeline_DrawVoronoiTriangle] = std::move(pipelines[0]);
			m_pipeline[Pipeline_DrawVoronoiPath] = std::move(pipelines[1]);
		}
	}

	void executeDrawVoronoiTriangle(vk::CommandBuffer cmd)
	{
		static int s_id;
		static int s_id_sub;

		s_id = (s_id_sub++ / 3) % 1024;

		vk::ImageMemoryBarrier image_barrier;
		image_barrier.setImage(m_render_target->m_image);
		image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, { image_barrier });

		{
			vk::RenderPassBeginInfo render_begin_info;
			render_begin_info.setRenderPass(m_render_pass.get());
			render_begin_info.setFramebuffer(m_framebuffer.get());
			render_begin_info.setRenderArea(vk::Rect2D({}, m_render_target->m_resolution));
			cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eInline);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_DrawVoronoiTriangle].get(), 0, m_context->getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
			cmd.pushConstants<uint>(m_pipeline_layout[PipelineLayout_DrawVoronoiTriangle].get(), vk::ShaderStageFlagBits::eGeometry, 0, s_id);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_DrawVoronoiTriangle].get());

			cmd.draw(1, 1, 0, 0);

			cmd.endRenderPass();

		}
	}
	void executeDrawVoronoiPath(vk::CommandBuffer cmd)
	{
		vk::ImageMemoryBarrier image_barrier;
		image_barrier.setImage(m_render_target->m_image);
		image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, { image_barrier });

		{
			vk::RenderPassBeginInfo render_begin_info;
			render_begin_info.setRenderPass(m_render_pass.get());
			render_begin_info.setFramebuffer(m_framebuffer.get());
			render_begin_info.setRenderArea(vk::Rect2D({}, m_render_target->m_resolution));
			cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eInline);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_DrawVoronoiTriangle].get(), 0, m_context->getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
//			cmd.pushConstants<uint>(m_pipeline_layout[PipelineLayout_DrawVoronoiTriangle].get(), vk::ShaderStageFlagBits::eGeometry, 0, 0u);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_DrawVoronoiPath].get());

			cmd.draw(1024, 1, 0, 0);

			cmd.endRenderPass();

		}
	}

	enum Shader
	{
		Shader_DrawVoronoiTriangle_VS,
		Shader_DrawVoronoiTriangle_GS,
		Shader_DrawVoronoiTriangle_FS,

		Shader_DrawVoronoiPath_GS,
		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_DrawVoronoiTriangle,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_DrawVoronoiTriangle,
		Pipeline_DrawVoronoiPath,
		Pipeline_Num,
	};
	std::array < vk::UniqueShaderModule, Shader_Num> m_shader;
 	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
 	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	std::shared_ptr<GI2DPhysics> m_context;
	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

};
