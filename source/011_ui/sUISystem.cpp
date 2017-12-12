#include <011_ui/sUISystem.h>

#include <string>
#include <cassert>
#include <chrono>
#include <thread>

#include <applib/DrawHelper.h>
#include <applib/sSystem.h>
#include <applib/sImGuiRenderer.h>

sUISystem::sUISystem(const std::shared_ptr<btr::Context>& context)
{
	m_context = context;
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	m_render_pass = std::make_shared<RenderBackbufferModule>(context);
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
		cmd->copyBuffer(staging.getInfo().buffer, m_global.getInfo().buffer, copy);

		{
			auto to_read = m_global.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_read }, {});
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
		};
		m_descriptor_set_layout = btr::Descriptor::createDescriptorSetLayout(context, binding);
		m_descriptor_pool = btr::Descriptor::createDescriptorPool(context, binding, 30);

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
		};
		m_descriptor_set_layout_anime = btr::Descriptor::createDescriptorSetLayout(context, binding);
		m_descriptor_pool_anime = btr::Descriptor::createDescriptorPool(context, binding, 30);

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
				sSystem::Order().getSystemDescriptor().getLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_CLEAR] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_descriptor_set_layout.get(),
				m_descriptor_set_layout_anime.get(),
				sSystem::Order().getSystemDescriptor().getLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
	}
	// pipeline
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


}

std::shared_ptr<UI> sUISystem::create(const std::shared_ptr<btr::Context>& context)
{
	auto ui = std::make_shared<UI>();
	{
		btr::BufferMemoryDescriptorEx<UIInfo> desc;
		desc.element_num = 1;
		ui->m_info = context->m_uniform_memory.allocateMemory(desc);

	}
	{
		btr::BufferMemoryDescriptorEx<UIParam> desc;
		desc.element_num = 1024;
		ui->m_object = context->m_storage_memory.allocateMemory(desc);
	}

	return ui;
}

vk::CommandBuffer sUISystem::draw()
{
	auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);

	m_context->m_device->resetDescriptorPool(m_descriptor_pool.get());
	for (auto& ui : m_render)
	{
		vk::DescriptorSetLayout layouts[] = { m_descriptor_set_layout.get() };
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.setDescriptorPool(m_descriptor_pool.get());
		alloc_info.setDescriptorSetCount(array_length(layouts));
		alloc_info.setPSetLayouts(layouts);
		auto descriptor_set = m_context->m_device->allocateDescriptorSets(alloc_info)[0];

		vk::DescriptorBufferInfo uniforms[] = {
			m_global.getInfo(),
			ui->m_info.getInfo(),
		};
		vk::DescriptorBufferInfo storages[] = {
			ui->m_object.getInfo(),
			ui->m_boundary.getInfo(),
			ui->m_work.getInfo(),
			ui->m_play_info.getInfo(),
		};

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
		};
		m_context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});

		{
			// 初期化
			{
				auto to_write = ui->m_info.makeMemoryBarrier();
				to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_write.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_CLEAR].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_CLEAR].get(), 0, { descriptor_set }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_CLEAR].get(), 1, { sSystem::Order().getSystemDescriptor().getSet() }, {});
			cmd.dispatch(1, 1, 1);
		}
		{
			// アニメーションする？
			if (ui->m_anime)
			{
				
				vk::DescriptorSetLayout layouts[] = { m_descriptor_set_layout_anime.get() };
				vk::DescriptorSetAllocateInfo alloc_info;
				alloc_info.setDescriptorPool(m_descriptor_pool_anime.get());
				alloc_info.setDescriptorSetCount(array_length(layouts));
				alloc_info.setPSetLayouts(layouts);
				auto descriptor_set_anime = m_context->m_device->allocateDescriptorSets(alloc_info)[0];

				vk::DescriptorBufferInfo uniforms[] = {
					ui->m_anime->m_num.getInfo(),
				};
				vk::DescriptorBufferInfo storages[] = {
					ui->m_anime->m_anim_info.getInfo(),
					ui->m_anime->m_anim_key.getInfo(),
					ui->m_work.getInfo(),
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
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(1)
					.setDstSet(descriptor_set_anime),
				};
				{
					auto to_read = ui->m_work.makeMemoryBarrier();
					to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
					to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
				}
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_LAYOUT_ANIMATION].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION].get(), 0, { descriptor_set }, {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION].get(), 1, { descriptor_set_anime }, {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION].get(), 2, { sSystem::Order().getSystemDescriptor().getSet() }, {});
				cmd.dispatch(1, 1, 1);
			}
		}
		{
			{
				auto to_read = ui->m_info.makeMemoryBarrier();
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
				auto to_read = ui->m_work.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_BOUNDARY].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY].get(), 0, { descriptor_set }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_BOUNDARY].get(), 1, { sSystem::Order().getSystemDescriptor().getSet() }, {});
			cmd.dispatch(1, 1, 1);

		}
		{
			{
				auto to_read = ui->m_work.makeMemoryBarrier();
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
	m_render.clear();
	cmd.end();
	return cmd;
}

void UIManipulater::drawtree(int32_t index)
{
	if (index == -1) { return; }
	bool is_open = ImGui::TreeNodeEx(m_object_tool[index].m_name.data(), m_last_select_index == index ? ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_OpenOnArrow : 0);
	if (ImGui::IsItemClicked(0)) {
		m_last_select_index = index;
	}
	if (is_open)
	{
		drawtree(m_object.getMappedPtr(index)->m_child_index);
		ImGui::TreePop();
	}
	drawtree(m_object.getMappedPtr(index)->m_chibiling_index);

}

void UIManipulater::animManip()
{
	if (ImGui::SmallButton(m_anim_manip->m_is_playing ? "STOP" : "PLAY")) {
		m_anim_manip->m_is_playing = !m_anim_manip->m_is_playing;
	}
	ImGui::SameLine();
	ImGui::DragInt("current frame", &m_anim_manip->m_frame, 0.1f, 0, 100);

	ImGui::Separator();

}

void UIManipulater::dataManip()
{
	UIAnimeData* data = m_anim_manip->m_anime->getData(m_object_tool[m_last_select_index].m_name.data());
	if (data)
	{
		auto& keys = data->m_key;

		if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
		{
			if (ImGui::BeginPopupContextWindow("key context"))
			{
				if (ImGui::Selectable("Add"))
				{
					UIAnimeKey new_key;
					new_key.m_flag = UIAnimeKey::is_enable;
					new_key.m_frame = m_anim_manip->m_frame;
					new_key.m_value_i = 0;
					keys.push_back(new_key);
				};
				if (ImGui::Selectable("Erase"))
				{
					for (auto it = keys.begin(); it != keys.end();)
					{
						if (btr::isOn(it->m_flag, UIAnimeKey::is_erase)) {
							it = keys.erase(it);
						}
						else {
							it++;
						}
					}
				};
				if (ImGui::Selectable("Sort"))
				{
					std::stable_sort(keys.begin(), keys.end(), [](auto&& a, auto&& b) { return a.m_frame < b.m_frame; });
				};
				if (ImGui::Selectable("Save"))
				{
					m_request_update_animation = true;
				};
				ImGui::EndPopup();
			}
			ImGui::Columns(4, "animcolumns");
			ImGui::Text("Frame"); ImGui::NextColumn();
			ImGui::Text("Value"); ImGui::NextColumn();
			ImGui::Text("Enable"); ImGui::NextColumn();
			ImGui::Text("Erase"); ImGui::NextColumn();
			ImGui::Separator();

			for (auto i = 0; i < keys.size(); i++)
			{
				char id[32] = {};
				sprintf_s(id, "key_%d", i);
				ImGui::PushID(id);

				auto& key = keys[i];

				sprintf_s(id, "frame_%d", i);
				ImGui::PushID(id);
				int32_t frame = key.m_frame;
				ImGui::DragInt("##frame", &frame, 0.1f, 0, 100);
				key.m_frame = frame;
				ImGui::NextColumn();
				ImGui::PopID();

				sprintf_s(id, "value_%d", i);
				ImGui::PushID(id);
				int value[2] = { key.m_value_i16[0], key.m_value_i16[1] };
				ImGui::DragInt2("##value", value, 0.1f);
				key.m_value_i16[0] = value[0];
				key.m_value_i16[1] = value[1];
				ImGui::NextColumn();
				ImGui::PopID();

				sprintf_s(id, "enable_%d", i);
				ImGui::PushID(id);
				ImGui::CheckboxFlags("##enable", &key.m_flag, UIAnimeKey::is_enable);
				ImGui::NextColumn();
				ImGui::PopID();

				sprintf_s(id, "erase_%d", i);
				ImGui::PushID(id);
				ImGui::CheckboxFlags("##erase", &key.m_flag, UIAnimeKey::is_erase);
				ImGui::NextColumn();
				ImGui::PopID();

				ImGui::PopID();
				ImGui::Separator();
			}
			ImGui::Columns(1);
			ImGui::EndChild();
		}
	}
	else
	{
		if (ImGui::BeginPopupContextWindow("key context"))
		{
			if (ImGui::Selectable("Make"))
			{
				UIAnimeData data;
				data.m_target_index = m_last_select_index;
				data.m_target_hash = m_object_tool[m_last_select_index].makeHash();
				m_anim_manip->m_anime->m_data.push_back(data);
			};
			ImGui::EndPopup();
		}

	}
}


vk::CommandBuffer UIManipulater::execute()
{
	auto func = [this]() 
	{
#if 1
		ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(200.f, 200.f), ImGuiCond_Once);
		if (ImGui::Begin(m_object_tool[0].m_name.data()))
		{
			if (ImGui::CollapsingHeader("Operate"))
			{
				if (ImGui::Button("addchild")) 
				{
					addnode(m_last_select_index);
				}
			}
			
			if (ImGui::CollapsingHeader("Info"))
			{
				if (m_last_select_index >= 0)
				{
					auto* param = m_object.getMappedPtr(m_last_select_index);
					ImGui::CheckboxFlags("IsSprite", &param->m_flag, is_sprite);
					m_request_update_boundary = ImGui::CheckboxFlags("IsBoundary", &param->m_flag, is_boundary);
					ImGui::CheckboxFlags("IsErase", &param->m_flag, is_trash);
					ImGui::InputFloat2("Pos", &param->m_position_local[0]);
					ImGui::InputFloat2("Size", &param->m_size_local[0]);
					ImGui::ColorPicker4("Color", &param->m_color_local[0]);
					ImGui::InputText("name", m_object_tool[m_last_select_index].m_name.data(), m_object_tool[m_last_select_index].m_name.size(), 0);
				}
			}

			{
				drawtree(0);
			}

			ImGui::End();
		}

		// アニメーションのウインドウ
		ImGui::SetNextWindowPos(ImVec2(10.f, 220.f), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(800.f, 200.f), ImGuiCond_Once);
		if (ImGui::Begin("anime"))
		{
			animManip();

			if (m_last_select_index >= 0) {
				dataManip();
			}

			ImGui::End();
		}


#else
		ImGui::ShowTestWindow();
#endif


	};
	sImGuiRenderer::Order().pushCmd(std::move(func));

	auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);
	{
		{
			auto to_write = m_ui->m_draw_cmd.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_write }, {});
		}
		cmd.updateBuffer<vk::DrawIndirectCommand>(m_ui->m_draw_cmd.getInfo().buffer, m_ui->m_draw_cmd.getInfo().offset, vk::DrawIndirectCommand( 4, m_object_counter, 0, 0 ));

		{
			auto to_read = m_ui->m_draw_cmd.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_read }, {});
		}

	}
	{
		{
			auto to_write = m_ui->m_object.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, {to_write}, {});
		}
		vk::BufferCopy copy;
		copy.setSrcOffset(m_object.getInfo().offset);
		copy.setDstOffset(m_ui->m_object.getInfo().offset);
		copy.setSize(m_ui->m_object.getInfo().range);
		cmd.copyBuffer(m_object.getInfo().buffer, m_ui->m_object.getInfo().buffer, copy);

		{
			auto to_read = m_ui->m_object.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, {to_read}, {});
		}
	}

	// boundaryの更新があった
	if (m_request_update_boundary)
	{
		m_boundary_counter = 0;
		btr::BufferMemoryDescriptorEx<UIBoundary> boundary_desc;
		boundary_desc.element_num = 1024;
		boundary_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = m_context->m_staging_memory.allocateMemory(boundary_desc);

		// 有効なバウンダリーをセット
		for (uint32_t i = 0; i < m_object_counter; i++)
		{
			auto* p = m_object.getMappedPtr(i);
			if (btr::isOn(p->m_flag, is_boundary)) {
				staging.getMappedPtr(m_boundary_counter)->m_param_index = i;
				staging.getMappedPtr(m_boundary_counter)->m_touch_time = 0.f;
				staging.getMappedPtr(m_boundary_counter)->m_flag = is_enable;
				m_boundary_counter++;
			}
		}

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_ui->m_boundary.getInfo().offset);
		copy.setSize(m_ui->m_boundary.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, m_ui->m_boundary.getInfo().buffer, copy);

		m_request_update_boundary = false;
	}

	int32_t max_depth = 0;
	for (uint32_t i = 0; i < m_object_counter; i++)
	{
		max_depth = glm::max(m_object.getMappedPtr(i)->m_depth, max_depth);
	}

	m_info.getMappedPtr()->m_depth_max = max_depth;
	m_info.getMappedPtr()->m_object_num = m_object_counter;
	m_info.getMappedPtr()->m_boundary_num = m_boundary_counter;
	{
		{
			auto to_write = m_ui->m_info.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_write }, {});
		}
		vk::BufferCopy copy;
		copy.setSrcOffset(m_info.getInfo().offset);
		copy.setDstOffset(m_ui->m_info.getInfo().offset);
		copy.setSize(m_ui->m_info.getInfo().range);
		cmd.copyBuffer(m_info.getInfo().buffer, m_ui->m_info.getInfo().buffer, copy);

		{
			auto to_read = m_ui->m_info.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_read }, {});
		}
	}

	if (m_request_update_animation)
	{
		m_request_update_animation = false;
		m_ui->m_anime = m_anim_manip->m_anime->makeResource(m_context, cmd);
	}

	sUISystem::Order().addRender(m_ui);

	cmd.end();
	return cmd;
}

