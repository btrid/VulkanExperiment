#include <011_ui/sUISystem.h>

#include <string>
#include <cassert>
#include <chrono>
#include <thread>

#include <applib/DrawHelper.h>
#include <applib/sSystem.h>
#include <applib/GraphicsResource.h>
#include <applib/sImGuiRenderer.h>


sUISystem::sUISystem(const std::shared_ptr<btr::Context>& context)
{

	m_context = context;
	m_render_pass = context->m_window->getRenderBackbufferPass();
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	{
		btr::BufferMemoryDescriptorEx<UIGlobal> desc;
		desc.element_num = 1;
		m_global = context->m_uniform_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		staging.getMappedPtr()->m_resolusion = uvec2(m_render_pass->getResolution().width, m_render_pass->getResolution().height);

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_global.getInfo().offset);
		copy.setSize(staging.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, m_global.getInfo().buffer, copy);

		{
			auto to_read = m_global.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_read }, {});
		}
	}

	// descriptor
	{
		auto stage = vk::ShaderStageFlagBits::eVertex| vk::ShaderStageFlagBits::eFragment|vk::ShaderStageFlagBits::eCompute;
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(5),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(6),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(7),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(8),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(UI::TEXTURE_MAX)
			.setBinding(9),
		};
		m_descriptor_set_layout = btr::Descriptor::createDescriptorSetLayout(context, binding);
		m_descriptor_pool = btr::Descriptor::createDescriptorPool(context, binding, 100);

	}
	{
		auto stage = vk::ShaderStageFlagBits::eCompute;
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(UI::ANIME_NUM)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
		};
		m_descriptor_set_layout_anime = btr::Descriptor::createDescriptorSetLayout(context, binding);
		m_descriptor_pool_anime = btr::Descriptor::createDescriptorPool(context, binding, 100);

	}

	// setup shader
	{
		struct
		{
			const char* name;
		}shader_info[] =
		{
			{ "UIAnimation.comp.spv" },
			{ "UIClear.comp.spv" },
			{ "UIUpdate.comp.spv" },
			{ "UITransform.comp.spv" },
			{ "UIBoundary.comp.spv" },
			{ "UIRender.Vert.spv" },
			{ "UIRender.frag.spv" },
		};
		static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++) {
			m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
		}
	}

	// pipeline layout
	{
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_descriptor_set_layout.get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PIPELINE_LAYOUT_TRANSFORM] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_descriptor_set_layout.get(),
				sSystem::Order().getSystemDescriptorLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
		}
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_descriptor_set_layout.get(),
				m_descriptor_set_layout_anime.get(),
				sSystem::Order().getSystemDescriptorLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PIPELINE_LAYOUT_CLEAR] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
	}
	// pipeline
	m_render_pass = context->m_window->getRenderBackbufferPass();
	{
		{
			vk::PipelineShaderStageCreateInfo shader_info[SHADER_NUM];
			for (size_t i = 0; i < SHADER_NUM; i++) {
				shader_info[i].setModule(m_shader_module[i].get());
				shader_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[i].setPName("main");
			}

			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_ANIMATION])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_CLEAR])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_CLEAR].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_UPDATE])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_TRANSFORM])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_TRANSFORM].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_BOUNDARY])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY].get()),
			};
			auto pipelines = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PIPELINE_ANIMATION] = std::move(pipelines[0]);
			m_pipeline[PIPELINE_CLEAR] = std::move(pipelines[1]);
			m_pipeline[PIPELINE_UPDATE] = std::move(pipelines[2]);
			m_pipeline[PIPELINE_TRANSFORM] = std::move(pipelines[3]);
			m_pipeline[PIPELINE_BOUNDARY] = std::move(pipelines[4]);
		}
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleStrip);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)m_render_pass->getResolution().width, (float)m_render_pass->getResolution().height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_render_pass->getResolution().width, m_render_pass->getResolution().height));
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

			vk::PipelineColorBlendAttachmentState blend_state;
			blend_state.setBlendEnable(VK_TRUE);
			blend_state.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
			blend_state.setAlphaBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
			blend_state.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
			blend_state.setColorBlendOp(vk::BlendOp::eAdd);
			blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA);
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(1);
			blend_info.setPAttachments(&blend_state);

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader_module[SHADER_VERT_RENDER].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader_module[SHADER_FRAG_RENDER].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get())
				.setRenderPass(m_render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PIPELINE_RENDER] = std::move(pipelines[0]);

		}

	}

	{
		btr::BufferMemoryDescriptorEx<UIWork> desc;
		desc.element_num = 10000;
		m_work = context->m_storage_memory.allocateMemory(desc);
	}

}

vk::CommandBuffer sUISystem::draw()
{
	auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);

	auto& renders = m_render[sGlobal::Order().getRenderIndex()];
	std::vector<vk::UniqueDescriptorSet> descriptor_holder;
	descriptor_holder.reserve(renders.size() * 2);
	for (uint32_t i = 0; i < renders.size(); i++)
	{
		auto& ui = renders[i];
		vk::DescriptorSet descriptor_set;
		vk::DescriptorSet descriptor_set_anime;

//		continue;
		{
			vk::DescriptorSetLayout layouts[] = { m_descriptor_set_layout.get() };
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.setDescriptorPool(m_descriptor_pool.get());
			alloc_info.setDescriptorSetCount(array_length(layouts));
			alloc_info.setPSetLayouts(layouts);
			descriptor_holder.emplace_back(std::move(m_context->m_device->allocateDescriptorSetsUnique(alloc_info)[0]));
			descriptor_set = descriptor_holder.back().get();

			vk::DescriptorBufferInfo uniforms[] = {
				m_global.getInfo(),
				ui->m_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				ui->m_object.getInfo(),
				ui->m_boundary.isValid() ? ui->m_boundary.getInfo() : m_context->m_storage_memory.getDummyInfo(),
				m_work.getInfo(),
				ui->m_play_info.getInfo(),
				ui->m_user_id.getInfo(),
				ui->m_scene.getInfo(),
				ui->m_scene.getInfo(),
			};
			vk::DescriptorImageInfo textures[UI::TEXTURE_MAX];
			auto default_texture = vk::DescriptorImageInfo(
				sGraphicsResource::Order().getWhiteTexture().m_sampler.get(),
				sGraphicsResource::Order().getWhiteTexture().m_image_view.get(),
				vk::ImageLayout::eShaderReadOnlyOptimal);
			std::fill(std::begin(textures), std::end(textures), default_texture);
			for (size_t i = 0; i < ui->m_textures.size(); i++)
			{
				if (ui->m_textures[i].isReady()) {
					textures[i].imageView = ui->m_textures[i].getImageView();
				}
			}

			auto write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(descriptor_set),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(2)
				.setDstSet(descriptor_set),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(array_length(textures))
				.setPImageInfo(textures)
				.setDstBinding(9)
				.setDstSet(descriptor_set),
			};
			m_context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});
		}
		{
			vk::DescriptorSetLayout layouts_anime[] = { m_descriptor_set_layout_anime.get() };
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.setDescriptorPool(m_descriptor_pool.get());
			alloc_info.setDescriptorSetCount(array_length(layouts_anime));
			alloc_info.setPSetLayouts(layouts_anime);

			descriptor_holder.emplace_back(std::move(m_context->m_device->allocateDescriptorSetsUnique(alloc_info)[0]));
			descriptor_set_anime = descriptor_holder.back().get();

			vk::DescriptorBufferInfo uniforms[] = {
				ui->m_anime_list[0]->m_anime_info.getInfo(),
			};

			static vk::DescriptorBufferInfo AnimeDataInfo_default(m_context->m_storage_memory.getBuffer(), 0, sizeof(UIAnimeKeyInfo));
			vk::DescriptorBufferInfo datainfos[UI::ANIME_NUM];
			std::fill(std::begin(datainfos), std::end(datainfos), AnimeDataInfo_default);
			datainfos[0] = ui->m_anime_list[0]->m_anime_data_info.getInfo();

			vk::DescriptorBufferInfo keys[] = {
				ui->m_anime_list[0]->m_anime_key.getInfo(),
			};
			auto write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(descriptor_set_anime),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(datainfos))
				.setPBufferInfo(datainfos)
				.setDstArrayElement(0)
				.setDstBinding(1)
				.setDstSet(descriptor_set_anime),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(keys))
				.setPBufferInfo(keys)
				.setDstBinding(2)
				.setDstSet(descriptor_set_anime),
			};
			m_context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});
		}

		{
			// 初期化
			{
				auto to_write = ui->m_info.makeMemoryBarrier();
				to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_write.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});
			}
			{
				auto to_write = ui->m_play_info.makeMemoryBarrier();
				to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});
			}
			{
				auto to_read = m_work.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_CLEAR].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_CLEAR].get(), 0, { descriptor_set }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_CLEAR].get(), 1, { descriptor_set_anime }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_CLEAR].get(), 2, { sSystem::Order().getSystemDescriptorSet() }, {0});
			cmd.dispatch(1, 1, 1);

			cmd.fillBuffer(ui->m_scene.getInfo().buffer, ui->m_scene.getInfo().offset, ui->m_scene.getInfo().range, 0u);
		}
		{
			// animation
			{
				auto to_read = m_work.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_ANIMATION].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION].get(), 0, { descriptor_set }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION].get(), 1, { descriptor_set_anime }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION].get(), 2, { sSystem::Order().getSystemDescriptorSet() }, { 0 });
			cmd.dispatch(1, 1, 1);

		}
		{
			{
				auto to_read = m_work.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_LAYOUT_TRANSFORM].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_TRANSFORM].get(), 0, { descriptor_set }, {});
			cmd.dispatch(1, 1, 1);
		}

		{
			// バウンダリー更新
			{
				auto to_read = m_work.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_BOUNDARY].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY].get(), 0, { descriptor_set }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY].get(), 1, { descriptor_set_anime }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY].get(), 2, { sSystem::Order().getSystemDescriptorSet() }, { 0 });
			cmd.dispatch(1, 1, 1);

		}
		{
			{
				auto to_read = m_work.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_read }, {});
			}

			vk::RenderPassBeginInfo begin_render_info;
			begin_render_info.setFramebuffer(m_render_pass->getFramebuffer(m_context->getGPUFrame()));
			begin_render_info.setRenderPass(m_render_pass->getRenderPass());
			begin_render_info.setRenderArea(vk::Rect2D({}, m_render_pass->getResolution()));
			cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_RENDER].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_RENDER].get(), 0, { descriptor_set }, {});

			cmd.drawIndirect(ui->m_draw_cmd.getInfo().buffer, ui->m_draw_cmd.getInfo().offset, 1, ui->m_draw_cmd.getDataSizeof());

			cmd.endRenderPass();
		}

	}
	renders.clear();
	sDeleter::Order().enque(std::move(descriptor_holder));
	cmd.end();
	return cmd;
}

