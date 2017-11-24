#include <011_ui/sUISystem.h>

#include <string>
#include <cassert>
#include <chrono>
#include <thread>

#include <applib/DrawHelper.h>
#include <applib/sImGuiRenderer.h>

void sUISystem::setup(const std::shared_ptr<btr::Context>& context)
{
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
		copy.setSrcOffset(staging.getInfo().range);
		cmd->copyBuffer(staging.getInfo().buffer, m_global.getInfo().buffer, copy);
	}

	// descriptor
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
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
		};
		m_descriptor_set_layout = btr::Descriptor::createDescriptorSetLayout(context, binding);
		m_descriptor_pool = btr::Descriptor::createDescriptorPool(context, binding, 30);

	}


	// setup shader
	{
		struct
		{
			const char* name;
		}shader_info[] =
		{
			{ "UIAnimation.comp.spv" },
			{ "UIUpdate.comp.spv" },
			{ "UITransform.comp.spv" },
			{ "UIRender.Vert.spv" },
			{ "UIRender.frag.spv" },
//			{ "UIManipurater.comp.spv" },
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
			m_pipeline_layout[PIPELINE_LAYOUT_ANIMATION] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PIPELINE_LAYOUT_TRANSFORM] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
	}
	// pipeline
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
			.setStage(shader_info[SHADER_UPDATE])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[SHADER_TRANSFORM])
			.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_TRANSFORM].get()),
		};
		auto pipelines = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		m_pipeline[SHADER_ANIMATION] = std::move(pipelines[0]);
		m_pipeline[SHADER_UPDATE] = std::move(pipelines[1]);
		m_pipeline[SHADER_TRANSFORM] = std::move(pipelines[2]);

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
	{

		vk::DescriptorSetLayout layouts[] = { m_descriptor_set_layout.get() };
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.setDescriptorPool(m_descriptor_pool.get());
		alloc_info.setDescriptorSetCount(array_length(layouts));
		alloc_info.setPSetLayouts(layouts);
		ui->m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(alloc_info)[0]);

		vk::DescriptorBufferInfo uniforms[] = {
			m_global.getInfo(),
			ui->m_info.getInfo(),
		};
		vk::DescriptorBufferInfo storages[] = {
			ui->m_object.getInfo(),
		};

		auto write_desc =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(array_length(uniforms))
			.setPBufferInfo(uniforms)
			.setDstBinding(0)
			.setDstSet(ui->m_descriptor_set.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(2)
			.setDstSet(ui->m_descriptor_set.get()),
		};
		context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});
	}

	return ui;
}

vk::CommandBuffer sUISystem::draw(const std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
	cmd.end();
	return cmd;
}

void UIManipulater::test()
{
	auto func = [this]() {
// 		ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Once);
// 		ImGui::SetNextWindowSize(ImVec2(200.f, 400.f), ImGuiCond_Once);
// 		m_ui->m_name = "new";
// 		ImGui::Begin(m_ui->m_name.c_str());
// 		if (ImGui::CollapsingHeader("Help")) 
// 		{
// 			ImGui::Text("new_child");
// 			ImGui::SameLine();
// 			if (ImGui::Button("", ImVec2(10.f, 10.f)))
// 			{
// 				int a = 0; a++;
// 			}
// 		}
// 		ImGui::End();
		ImGui::ShowTestWindow();
	};
	sImGuiRenderer::Order().pushCmd(std::move(func));
}
