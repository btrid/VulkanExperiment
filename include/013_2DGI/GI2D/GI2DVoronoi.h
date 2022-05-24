#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>
#include <013_2DGI/GI2D/GI2DPhysics.h>

struct GI2DVoronoi
{
#define COLLIDABLE_NUM (4)
	enum Shader
	{

		Shader_Voronoi_SetupJFA,
		Shader_Voronoi_MakeJFA,
		Shader_Voronoi_MakeTriangle,
		Shader_Voronoi_SortTriangleVertex,

		Shader_Voronoi_MakePath,

		Shader_Voronoi_DestructWall_VS,
		Shader_Voronoi_DestructWall_GS,
		Shader_Voronoi_DestructWall_FS,


		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_DestructWall,
		PipelineLayout_Voronoi,

		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Voronoi_SetupJFA,
		Pipeline_Voronoi_MakeJFA,
		Pipeline_Voronoi_MakeTriangle,
		Pipeline_Voronoi_SortTriangleVertex,
		Pipeline_Voronoi_MakePath,

		Pipeline_MakeRB_DestructWall,
		Pipeline_Num,
	};

	enum DescriptorLayout
	{
		DescLayout_Data,
		DescLayout_Num,
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

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorLayout i)const { return m_desc_layout[i].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorLayout i)const { return m_descset[i].get(); }

	GI2DVoronoi(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DPhysics>& physics_context)
	{
 		m_context = context;
 		m_physics_context = physics_context;

		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_desc_layout[DescLayout_Data] = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		{
			const char* name[] =
			{
				"Voronoi_SetupJFA.comp.spv",
				"Voronoi_MakeJFA.comp.spv",
				"Voronoi_MakeTriangle.comp.spv",
				"Voronoi_SortTriangle.comp.spv",
				"Voronoi_MakePath.comp.spv",

				"Voronoi_DestructWall.vert.spv",
				"Voronoi_DestructWall.geom.spv",
				"Voronoi_DestructWall.frag.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(m_context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_physics_context->getDescriptorSetLayout(GI2DPhysics::DescLayout_Data),
				m_physics_context->m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				m_physics_context->getDescriptorSetLayout(GI2DPhysics::DescLayout_Make),
				m_desc_layout[DescLayout_Data].get(),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_DestructWall] = m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
			vk::DebugUtilsObjectNameInfoEXT name_info;
			name_info.pObjectName = "PipelineLayout_DestructWall";
			name_info.objectType = vk::ObjectType::ePipelineLayout;
			name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_DestructWall].get());
			m_context->m_device.setDebugUtilsObjectNameEXT(name_info);
#endif
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_desc_layout[DescLayout_Data].get(),
			};
			vk::PushConstantRange ranges[] = {
				vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, 16),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
			pipeline_layout_info.setPPushConstantRanges(ranges);
			m_pipeline_layout[PipelineLayout_Voronoi] = m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
			vk::DebugUtilsObjectNameInfoEXT name_info;
			name_info.pObjectName = "PipelineLayout_Voronoi";
			name_info.objectType = vk::ObjectType::ePipelineLayout;
			name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_Voronoi].get());
			m_context->m_device.setDebugUtilsObjectNameEXT(name_info);
#endif
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 11> shader_info;
			shader_info[0].setModule(m_shader[Shader_Voronoi_SetupJFA].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_Voronoi_MakeJFA].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_Voronoi_MakeTriangle].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_Voronoi_SortTriangleVertex].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader[Shader_Voronoi_MakePath].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[4])
				.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
			};

			m_pipeline[Pipeline_Voronoi_SetupJFA] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_Voronoi_MakeJFA] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
			m_pipeline[Pipeline_Voronoi_MakeTriangle] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
			m_pipeline[Pipeline_Voronoi_SortTriangleVertex] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
			m_pipeline[Pipeline_Voronoi_MakePath] = m_context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[4]).value;
		}

		// graphics pipeline
		{
			// レンダーパス
			{
				// sub pass
				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(0);
				subpass.setPColorAttachments(nullptr);

				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(0);
				renderpass_info.setPAttachments(nullptr);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);

				m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);
			}
			{
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass.get());
				framebuffer_info.setAttachmentCount(0);
				framebuffer_info.setPAttachments(nullptr);
				framebuffer_info.setWidth(1024);
				framebuffer_info.setHeight(1024);
				framebuffer_info.setLayers(1);

				m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}

			{
				vk::PipelineShaderStageCreateInfo shader_info[] =
				{
					vk::PipelineShaderStageCreateInfo()
					.setModule(m_shader[Shader_Voronoi_DestructWall_VS].get())
					.setPName("main")
					.setStage(vk::ShaderStageFlagBits::eVertex),
					vk::PipelineShaderStageCreateInfo()
					.setModule(m_shader[Shader_Voronoi_DestructWall_GS].get())
					.setPName("main")
					.setStage(vk::ShaderStageFlagBits::eGeometry),
					vk::PipelineShaderStageCreateInfo()
					.setModule(m_shader[Shader_Voronoi_DestructWall_FS].get())
					.setPName("main")
					.setStage(vk::ShaderStageFlagBits::eFragment),
				};

				// assembly
				vk::PipelineInputAssemblyStateCreateInfo assembly_info;
				assembly_info.setPrimitiveRestartEnable(VK_FALSE);
				assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

				// viewport
				vk::Viewport viewport = vk::Viewport(0.f, 0.f, 1024.f, 1024.f, 0.f, 1.f);
				vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(1024, 1024));
				vk::PipelineViewportStateCreateInfo viewportInfo;
				viewportInfo.setViewportCount(1);
				viewportInfo.setPViewports(&viewport);
				viewportInfo.setScissorCount(1);
				viewportInfo.setPScissors(&scissor);

				vk::PipelineRasterizationStateCreateInfo rasterization_info;
				rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
				rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
				rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
				rasterization_info.setLineWidth(1.f);

				vk::PipelineMultisampleStateCreateInfo sample_info;
				sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

				vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
				depth_stencil_info.setDepthTestEnable(VK_FALSE);
				depth_stencil_info.setDepthWriteEnable(VK_FALSE);
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
				depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
				depth_stencil_info.setStencilTestEnable(VK_FALSE);

				std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
					vk::PipelineColorBlendAttachmentState()
					.setBlendEnable(VK_FALSE)
					.setColorWriteMask(vk::ColorComponentFlagBits::eR
						| vk::ColorComponentFlagBits::eG
						| vk::ColorComponentFlagBits::eB
						| vk::ColorComponentFlagBits::eA)
				};
				vk::PipelineColorBlendStateCreateInfo blend_info;
				blend_info.setAttachmentCount((uint32_t)blend_state.size());
				blend_info.setPAttachments(blend_state.data());

				vk::PipelineVertexInputStateCreateInfo vertex_input_info;

				std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
				{
					vk::GraphicsPipelineCreateInfo()
					.setStageCount(array_length(shader_info))
					.setPStages(shader_info)
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&assembly_info)
					.setPViewportState(&viewportInfo)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pipeline_layout[PipelineLayout_DestructWall].get())
					.setRenderPass(m_render_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info),
				};
				m_pipeline[Pipeline_MakeRB_DestructWall] = context->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info[0]).value;

			}

		}
		{
			b_voronoi_cell = m_context->m_storage_memory.allocateMemory<VoronoiCell>({ 4096,{} });
			b_voronoi_polygon = m_context->m_storage_memory.allocateMemory<VoronoiPolygon>({ 4096,{} });
			b_voronoi = m_context->m_storage_memory.allocateMemory<int16_t>({ physics_context->m_gi2d_context->m_desc.Resolution.x*physics_context->m_gi2d_context->m_desc.Resolution.y,{} });
			b_voronoi_vertex_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 1,{} });
			b_voronoi_vertex = m_context->m_storage_memory.allocateMemory<VoronoiVertex>({ 4096 * 6,{} });
			b_voronoi_path = m_context->m_storage_memory.allocateMemory<int16_t>({ 4096,{} });
			{
				vk::DescriptorSetLayout layouts[] = {
					m_desc_layout[DescLayout_Data].get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descset[DescLayout_Data] = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
					b_voronoi_cell.getInfo(),
					b_voronoi_polygon.getInfo(),
					b_voronoi.getInfo(),
					b_voronoi_vertex_counter.getInfo(),
					b_voronoi_vertex.getInfo(),
					b_voronoi_path.getInfo(),
				};

				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descset[DescLayout_Data].get()),
				};
				context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}
		auto cmd = m_context->m_cmd_pool->allocCmdTempolary(0);
		cmd.fillBuffer(b_voronoi_vertex_counter.getInfo().buffer, b_voronoi_vertex_counter.getInfo().offset, b_voronoi_vertex_counter.getInfo().range, 0);

	}
	void executeDestructWall(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		GI2DPhysics::Rigidbody rb;
		{
			{
				vk::BufferMemoryBarrier to_write[] = {
					m_physics_context->b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);
			}

			{
				rb.R = vec4(1.f, 0.f, 0.f, 1.f);
				rb.center_of_mass = vec2(0.f);
				rb.flag = RB_FLAG_FLUID;
				//			rb.flag = 0;
				rb.life = 100;
				rb.pnum = 0;
				rb.center_of_mass_work = ivec2(0);
				rb.Apq_work = ivec4(0);

				static uint s_id;
				GI2DPhysics::RBMakeParam make_param;
				make_param.pb_num = uvec4(0, 1, 1, 0);
				make_param.registered_num = uvec4(0, 1, 1, 0);
				make_param.rb_aabb = ivec4(0);
				make_param.destruct_voronoi_id = s_id;
				s_id = (s_id + 1) % 4096;

				cmd.updateBuffer<GI2DPhysics::Rigidbody>(m_physics_context->b_make_rigidbody.getInfo().buffer, m_physics_context->b_make_rigidbody.getInfo().offset, rb);
				cmd.updateBuffer<GI2DPhysics::RBMakeParam>(m_physics_context->b_make_param.getInfo().buffer, m_physics_context->b_make_param.getInfo().offset, make_param);
				cmd.fillBuffer(m_physics_context->b_make_jfa_cell.getInfo().buffer, m_physics_context->b_make_jfa_cell.getInfo().offset, m_physics_context->b_make_jfa_cell.getInfo().range, 0xffffffff);
			}

			{
				vk::BufferMemoryBarrier to_read[] = {
					m_physics_context->b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					m_physics_context->b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					m_physics_context->b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}

		}

		_label.insert("GI2DPhysics::DestructWall");
		{

			vk::DescriptorSet descriptorsets[] = {
				m_physics_context->getDescriptorSet(GI2DPhysics::DescLayout_Data),
				m_physics_context->m_gi2d_context->getDescriptorSet(),
				m_physics_context->getDescriptorSet(GI2DPhysics::DescLayout_Make),
				m_descset[DescLayout_Data].get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_DestructWall].get(), 0, array_length(descriptorsets), descriptorsets, 0, nullptr);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_MakeRB_DestructWall].get());

			vk::RenderPassBeginInfo render_begin_info;
			render_begin_info.setRenderPass(m_render_pass.get());
			render_begin_info.setFramebuffer(m_framebuffer.get());
			render_begin_info.setRenderArea(vk::Rect2D({}, vk::Extent2D(1024, 1024)));
			cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eInline);

			cmd.draw(1, 1, 0, 0);

			cmd.endRenderPass();

		}

		{
			vk::BufferMemoryBarrier to_read[] = {
				m_physics_context->b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_physics_context->b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_physics_context->b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}

		m_physics_context->_make(cmd);

	}


	void executeMakeVoronoi(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Voronoi].get(), 0, m_descset[DescLayout_Data].get(), {});

		ivec2 reso = m_physics_context->m_gi2d_context->m_desc.Resolution;
		int voronoi_cell_num = 0;
		{
			{
				{
					vk::BufferMemoryBarrier to_write[] = {
						b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite),
						b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);
				}

				cmd.fillBuffer(b_voronoi.getInfo().buffer, b_voronoi.getInfo().offset, b_voronoi.getInfo().range, -1);
				cmd.fillBuffer(b_voronoi_polygon.getInfo().buffer, b_voronoi_polygon.getInfo().offset, b_voronoi_polygon.getInfo().range, 0);
				cmd.fillBuffer(b_voronoi_vertex_counter.getInfo().buffer, b_voronoi_vertex_counter.getInfo().offset, b_voronoi_vertex_counter.getInfo().range, 0);

				{
					vk::BufferMemoryBarrier to_read[] = {
						b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
						b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
						b_voronoi_vertex_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
				}

				// 適当に点を打つ
				uint step = 32;
				uint area = step * 0.6f;
				uint offset = step * 0.2f;
				std::vector<VoronoiCell> points;
				points.reserve(4096);
				for (uint y = 0; y < reso.y; y += step)
				{
					for (uint x = 0; x < reso.x; x += step)
					{
						uint xx = x + std::rand() % area + offset;
						uint yy = (y + std::rand() % area + offset);
						VoronoiCell cell;
						cell.point = i16vec2(xx, yy);
						points.push_back(cell);

					}

				}
				assert(points.capacity() == 4096);
				voronoi_cell_num = points.size();
				cmd.updateBuffer<VoronoiCell>(b_voronoi_cell.getInfo().buffer, b_voronoi_cell.getInfo().offset, points);


				vk::BufferMemoryBarrier to_read[] = {
					b_voronoi_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

			}

			{
				// voronoiに初期の点を打つ
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_SetupJFA].get());
				cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_Voronoi].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ voronoi_cell_num, 0, reso });
				auto num = app::calcDipatchGroups(uvec3(voronoi_cell_num, 1, 1), uvec3(1024, 1, 1));
				cmd.dispatch(num.x, num.y, num.z);

			}

			{
				// jump flooding algorithmでボロノイ図生成
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_MakeJFA].get());
				auto num = app::calcDipatchGroups(uvec3(reso, 1), uvec3(8, 8, 1));

				uint reso_max = glm::max(reso.x, reso.y);
				for (int distance = reso_max >> 1; distance != 0; distance >>= 1)
				{
					vk::BufferMemoryBarrier to_read[] = {
						b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
						0, nullptr, array_length(to_read), to_read, 0, nullptr);

					cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_Voronoi].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ distance, 0, reso });
					cmd.dispatch(num.x, num.y, num.z);
				}

			}
		}

		// make triangle
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_MakeTriangle].get());
			auto num = app::calcDipatchGroups(uvec3(reso, 1), uvec3(8, 8, 1));

			cmd.dispatch(num.x, num.y, num.z);
		}


		// sort triangle vertex
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_voronoi_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_SortTriangleVertex].get());
			auto num = app::calcDipatchGroups(uvec3(4096, 1, 1), uvec3(64, 1, 1));

			cmd.dispatch(num.x, num.y, num.z);
		}

		vk::BufferMemoryBarrier to_read[] = {
			b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

	void executeMakeVoronoiPath(vk::CommandBuffer cmd)
	{
		// make path
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_voronoi_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Voronoi].get(), 0, m_descset[DescLayout_Data].get(), {});
			cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_Voronoi].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ 123, 345, uvec2(0) });
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_MakePath].get());

			cmd.dispatch(1, 1, 1);
		}

	}


	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DPhysics> m_physics_context;

	std::array<vk::UniqueDescriptorSetLayout, DescLayout_Num> m_desc_layout;
	std::array<vk::UniqueDescriptorSet, DescLayout_Num> m_descset;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	btr::BufferMemoryEx<VoronoiCell> b_voronoi_cell;
	btr::BufferMemoryEx<VoronoiPolygon> b_voronoi_polygon;
	btr::BufferMemoryEx<int16_t> b_voronoi;
	btr::BufferMemoryEx<uvec4> b_voronoi_vertex_counter;
	btr::BufferMemoryEx<VoronoiVertex> b_voronoi_vertex;
	btr::BufferMemoryEx<int16_t> b_voronoi_path;
};
