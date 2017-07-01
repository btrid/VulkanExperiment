#include <applib/cModelRenderPrivate.h>
#include <applib/cModelPipeline.h>


cModelRenderPrivate::cModelRenderPrivate() = default;
cModelRenderPrivate::~cModelRenderPrivate() = default;

void cModelRenderPrivate::setup(cModelPipeline& pipeline)
{

	// setup draw
	{
		cDevice graphics_device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_GRAPHICS];

		{
			vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
			descriptor_set_alloc_info.setDescriptorPool(pipeline.m_descriptor_pool);
			descriptor_set_alloc_info.setDescriptorSetCount(1);
			descriptor_set_alloc_info.setPSetLayouts(&pipeline.m_descriptor_set_layout[cModelPipeline::DESCRIPTOR_MODEL]);
			m_draw_descriptor_set_per_model = graphics_device->allocateDescriptorSets(descriptor_set_alloc_info)[0];

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
				.setDstSet(m_draw_descriptor_set_per_model),
			};
			graphics_device->updateDescriptorSets(drawWriteDescriptorSets, {});

		}
		{
			// mesh‚²‚Æ‚ÌXV
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = pipeline.m_descriptor_pool;
			allocInfo.descriptorSetCount = (uint32_t)m_model_resource->m_material.size();
			allocInfo.pSetLayouts = &pipeline.m_descriptor_set_layout[cModelPipeline::DESCRIPTOR_PER_MESH];
			m_draw_descriptor_set_per_mesh = graphics_device->allocateDescriptorSets(allocInfo);
			for (size_t i = 0; i < m_draw_descriptor_set_per_mesh.size(); i++)
			{
				auto& material = m_model_resource->m_material[m_model_resource->m_mesh[i].m_material_index];

				std::vector<vk::DescriptorImageInfo> color_image_info = {
					vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
				};
				std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount((uint32_t)color_image_info.size())
					.setPImageInfo(color_image_info.data())
					.setDstBinding(0)
					.setDstSet(m_draw_descriptor_set_per_mesh[i]),
				};
				graphics_device->updateDescriptorSets(drawWriteDescriptorSets, {});
			}
		}

	}

}
void cModelRenderPrivate::execute(cModelPipeline& renderer, vk::CommandBuffer& cmd)
{
	m_playlist.execute();
	updateNodeTransform(0, m_world);
	updateBoneTransform(0);
	
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, m_bone_buffer.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite), {});
	m_bone_buffer.update(cmd);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, m_bone_buffer.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead), {});
}
void cModelRenderPrivate::draw(cModelPipeline& pipeline, vk::CommandBuffer& cmd)
{

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.m_graphics_pipeline);
	for (auto m : m_model_resource->m_mesh)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER], 0, m_draw_descriptor_set_per_model, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER], 1, m_draw_descriptor_set_per_mesh[m.m_material_index], {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER], 2, pipeline.m_descriptor_set_scene, {});
		cmd.pushConstants<glm::mat4>(pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER], vk::ShaderStageFlagBits::eVertex, 0, m_world);
		cmd.pushConstants<uint32_t>(pipeline.m_pipeline_layout[cModelPipeline::PIPELINE_LAYOUT_RENDER], vk::ShaderStageFlagBits::eFragment, 64, m.m_material_index);
		cmd.bindVertexBuffers(0, { m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
		cmd.bindIndexBuffer(m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, m_model_resource->m_mesh_resource.mIndexType);
		cmd.drawIndexedIndirect(m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, m_model_resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));
	}
}
