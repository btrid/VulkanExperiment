#include <applib/cModelRenderPrivate.h>
#include <applib/cModelPipeline.h>
#include <applib/DrawHelper.h>


cModelRenderPrivate::cModelRenderPrivate() = default;
cModelRenderPrivate::~cModelRenderPrivate() = default;

void cModelRenderPrivate::setup(cModelPipeline& pipeline)
{

	// setup draw
	{
		auto& gpu = sGlobal::Order().getGPU(0);
		auto& device = gpu.getDevice();

		{
			vk::DescriptorSetLayout layouts[] = 
			{
				pipeline.m_descriptor_set_layout[cModelPipeline::DESCRIPTOR_MODEL].get() 
			};

			vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
			descriptor_set_alloc_info.setDescriptorPool(pipeline.m_descriptor_pool.get());
			descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
			descriptor_set_alloc_info.setPSetLayouts(layouts);
			m_draw_descriptor_set_per_model = std::move(device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);

			std::vector<vk::DescriptorBufferInfo> storages = {
				m_bone_buffer.getBufferInfo(),
				m_material.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount((uint32_t)storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(0)
				.setDstSet(m_draw_descriptor_set_per_model.get()),
			};
			device->updateDescriptorSets(drawWriteDescriptorSets, {});
		}
		{
			// mesh‚²‚Æ‚ÌXV
			vk::DescriptorSetLayout layouts[] = {
				pipeline.m_descriptor_set_layout[cModelPipeline::DESCRIPTOR_PER_MESH].get(),
			};
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = pipeline.m_descriptor_pool.get();
			allocInfo.descriptorSetCount = array_length(layouts);
			allocInfo.pSetLayouts = layouts;
			m_draw_descriptor_set_per_mesh = std::move(device->allocateDescriptorSetsUnique(allocInfo)[0]);

			std::vector<vk::DescriptorImageInfo> color_image_info(cModelPipeline::DESCRIPTOR_TEXTURE_NUM, vk::DescriptorImageInfo(DrawHelper::Order().getWhiteTexture().m_sampler, DrawHelper::Order().getWhiteTexture().m_image_view, vk::ImageLayout::eShaderReadOnlyOptimal));
			for (size_t i = 0; i < m_model_resource->m_mesh.size(); i++)
			{
				auto& material = m_model_resource->m_material[m_model_resource->m_mesh[i].m_material_index];
				color_image_info[i] = vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
			}
			std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount((uint32_t)color_image_info.size())
				.setPImageInfo(color_image_info.data())
				.setDstBinding(0)
				.setDstSet(m_draw_descriptor_set_per_mesh.get()),
			};
			device->updateDescriptorSets(drawWriteDescriptorSets, {});
		}

	}

	{
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_buffer_info.commandPool = sThreadLocal::Order().getCmdPool(sGlobal::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;

		m_transfer_cmd = sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info);
		for (size_t i = 0; i < m_transfer_cmd.size(); i++)
		{
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			vk::CommandBufferInheritanceInfo inheritance_info;
			begin_info.setPInheritanceInfo(&inheritance_info);

			auto& cmd = m_transfer_cmd[i].get();
			cmd.begin(begin_info);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, m_bone_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite), {});

			vk::BufferCopy copy;
			copy.dstOffset = m_bone_buffer.getBufferInfo().offset;
			copy.size = m_bone_buffer.getBufferInfo().range;
			copy.srcOffset = m_bone_buffer_transfer[i].getBufferInfo().offset;
			cmd.copyBuffer(m_bone_buffer_transfer[i].getBufferInfo().buffer, m_bone_buffer.getBufferInfo().buffer, copy);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, m_bone_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead), {});
			cmd.end();
		}

	}

	{
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_buffer_info.commandPool = sThreadLocal::Order().getCmdPool(sGlobal::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
		m_graphics_cmd = sGlobal::Order().getGPU(0).getDevice()->allocateCommandBuffersUnique(cmd_buffer_info);

		for (size_t i = 0; i < m_graphics_cmd.size(); i++)
		{
			auto& cmd = m_graphics_cmd[i].get();
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
			vk::CommandBufferInheritanceInfo inheritance_info;
			inheritance_info.setFramebuffer(pipeline.m_framebuffer[i].get());
			inheritance_info.setRenderPass(pipeline.m_render_pass.get());
			begin_info.pInheritanceInfo = &inheritance_info;

			cmd.begin(begin_info);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.m_graphics_pipeline.get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER].get(), 0, m_draw_descriptor_set_per_model.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER].get(), 1, m_draw_descriptor_set_per_mesh.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER].get(), 2, pipeline.m_descriptor_set_scene.get(), {});
			cmd.pushConstants<glm::mat4>(pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER].get(), vk::ShaderStageFlagBits::eVertex, 0, m_model_transform.calcGlobal());
			cmd.bindVertexBuffers(0, { m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
			cmd.bindIndexBuffer(m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, m_model_resource->m_mesh_resource.mIndexType);
			for (auto& m : m_model_resource->m_mesh)
			{
//				cmd.pushConstants<uint32_t>(pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER].get(), vk::ShaderStageFlagBits::eFragment, 64, m.m_material_index);
				cmd.drawIndexedIndirect(m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, m_model_resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));
//				cmd.drawIndexedIndirect(m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset + sizeof(cModel::Mesh)*i, 1, sizeof(cModel::Mesh));
			}
			cmd.end();
		}
	}

}
void cModelRenderPrivate::execute(std::shared_ptr<btr::Executer>& executer, vk::CommandBuffer& cmd)
{
	cmd.executeCommands(m_transfer_cmd[executer->getGPUFrame()].get());
}
void cModelRenderPrivate::draw(std::shared_ptr<btr::Executer>& executer, vk::CommandBuffer& cmd)
{
	cmd.executeCommands(m_graphics_cmd[executer->getGPUFrame()].get());
}
