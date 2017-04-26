#include <002_model/cModelRender.h>
#include <002_model/cModelRenderer.h>

void ModelRender::setup(cModelRenderer&  renderer)
{
	// setup draw
	{
		cDevice graphics_device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_GRAPHICS];
		auto& pipeline = renderer.getDrawPipeline();

		// update desctiptor
		{
			// モデルごとのDescriptorの設定
			vk::DescriptorSetAllocateInfo alloc_info = vk::DescriptorSetAllocateInfo()
				.setDescriptorPool(pipeline.m_descriptor_pool)
				.setDescriptorSetCount(1)
				.setPSetLayouts(&pipeline.m_descriptor_set_layout[cModelRenderer::cModelDrawPipeline::DESCRIPTOR_SET_LAYOUT_PER_MODEL]);
			m_draw_descriptor_set_per_model = graphics_device->allocateDescriptorSets(alloc_info)[0];

			std::vector<vk::DescriptorBufferInfo> uniformBufferInfo = {
				m_resource->getBuffer(cModel::cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};

			std::vector<vk::DescriptorBufferInfo> storagemBufferInfo = {
//				m_resource->getBuffer(cModel::Resource::ModelBuffer::VS_MATERIAL).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_TRANSFORM).getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> fStoragemBufferInfo = {
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::MATERIAL).getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_descriptor_set =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniformBufferInfo.size())
				.setPBufferInfo(uniformBufferInfo.data())
				.setDstBinding(0)
				.setDstSet(m_draw_descriptor_set_per_model),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storagemBufferInfo.size())
				.setPBufferInfo(storagemBufferInfo.data())
				.setDstBinding(1)
				.setDstSet(m_draw_descriptor_set_per_model),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(fStoragemBufferInfo.size())
				.setPBufferInfo(fStoragemBufferInfo.data())
				.setDstBinding(16)
				.setDstSet(m_draw_descriptor_set_per_model),
			};

			graphics_device->updateDescriptorSets(write_descriptor_set, {});
		}
		{
			// meshごとの更新
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = pipeline.m_descriptor_pool;
			allocInfo.descriptorSetCount = m_resource->mMeshNum;
			allocInfo.pSetLayouts = &pipeline.m_descriptor_set_layout[cModelRenderer::cModelDrawPipeline::DESCRIPTOR_SET_LAYOUT_PER_MESH];
			m_draw_descriptor_set_per_mesh = graphics_device->allocateDescriptorSets(allocInfo);
			for (size_t i = 0; i < m_draw_descriptor_set_per_mesh.size(); i++)
			{
				auto& material = m_resource->m_material[m_resource->m_material_index[i]];

				std::vector<vk::DescriptorImageInfo> color_image_info = {
					vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
				};
				std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(color_image_info.size())
					.setPImageInfo(color_image_info.data())
					.setDstBinding(32)
					.setDstSet(m_draw_descriptor_set_per_mesh[i]),
				};
				graphics_device->updateDescriptorSets(drawWriteDescriptorSets, {});
			}

		}


	}

	// setup execute
	{
		cDevice compute_device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_COMPUTE];

		auto& pipeline = renderer.getComputePipeline();
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(pipeline.m_descriptor_pool);
		descriptor_set_alloc_info.setDescriptorSetCount(pipeline.m_descriptor_set_layout.size());
		descriptor_set_alloc_info.setPSetLayouts(pipeline.m_descriptor_set_layout.data());
		m_compute_descriptor_set = compute_device->allocateDescriptorSets(descriptor_set_alloc_info);

		vk::WriteDescriptorSet desc;
		// Clear
		{
			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
				m_resource->mMesh.m_indirect_buffer.getBufferInfo(),
			};

			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[0]);
			compute_device->updateDescriptorSets(desc, {});

		}
		// AnimationUpdate
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[1]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource->getMotionBuffer(cAnimation::ANIMATION_INFO).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::PLAYING_ANIMATION).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[1]);
			compute_device->updateDescriptorSets(desc, {});

		}
		// MotionUpdate
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[2]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource->getMotionBuffer(cAnimation::ANIMATION_INFO).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::PLAYING_ANIMATION).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[2]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorImageInfo> images =
			{
				vk::DescriptorImageInfo()
				.setImageView(m_resource->getAnimation().m_motion_texture.getImageView())
				.setSampler(m_resource->getAnimation().m_motion_texture.m_private->m_sampler)
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(images.size())
				.setPImageInfo(images.data())
				.setDstBinding(32)
				.setDstSet(m_compute_descriptor_set[2]);
			compute_device->updateDescriptorSets(desc, {});
		}

		// NodeTransform
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[3]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::WORLD).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::NODE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[3]);
			compute_device->updateDescriptorSets(desc, {});
		}

		// CameraCulling			
		{
			std::vector<vk::DescriptorBufferInfo> uniforms = {};
			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::WORLD).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::NODE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_MAP).getBufferInfo(),
				m_resource->mMesh.m_indirect_buffer.getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[4]);
			compute_device->updateDescriptorSets(desc, {});
		}

		// BoneTransform
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[5]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_MAP).getBufferInfo(),
				m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_TRANSFORM).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[5]);
			compute_device->updateDescriptorSets(desc, {});
		}
	}
}
void ModelRender::execute(cModelRenderer& renderer, vk::CommandBuffer& cmd)
{
	// bufferの更新
	{
		std::vector<glm::mat4> world;
		world.reserve(m_model.size());
		for (auto& m : m_model)
		{
			world.push_back(m->getInstance()->m_world);
		}
		m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::WORLD).subupdate(world.data(), vector_sizeof(world), 0);
		m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::WORLD).copyTo();

		int32_t instance_num = m_model.size();
		m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).subupdate(&instance_num, offsetof(cModel::ModelInfo, mInstanceAliveNum), 4);
		m_resource->getBuffer(cModel::Resource::ModelStorageBuffer::MODEL_INFO).copyTo();
	}

	std::vector<vk::BufferMemoryBarrier> to_clear_barrier =
	{
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(m_resource->mMesh.m_indirect_buffer.getBuffer())
		.setSize(m_resource->mMesh.m_indirect_buffer.getBufferSize()),
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_TRANSFORM).getBuffer())
		.setSize(m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_TRANSFORM).getBufferSize())
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_clear_barrier, {});

	auto& pipeline = renderer.getComputePipeline();
	for (size_t i = 0; i < pipeline.m_pipeline_layout.size(); i++)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline[i]);
	 	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[i], 0, m_compute_descriptor_set[i], {});
		cmd.dispatchIndirect(m_resource->m_compute_indirect_buffer.getBuffer(), i * 12);

		if (i == 4) 
		{
			// 
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource->getBuffer(cModel::cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBuffer())
				.setSize(m_resource->getBuffer(cModel::cModel::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo().range),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(), {}, barrier, {});

		}
	}
	std::vector<vk::BufferMemoryBarrier> to_draw_barrier =
	{
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
		.setBuffer(m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_TRANSFORM).getBuffer())
		.setSize(m_resource->getBuffer(cModel::Resource::ModelConstantBuffer::BONE_TRANSFORM).getBufferInfo().range),
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
		.setBuffer(m_resource->mMesh.m_indirect_buffer.getBuffer())
		.setSize(m_resource->mMesh.m_indirect_buffer.getBufferSize())
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_draw_barrier, {});

}

void ModelRender::draw(cModelRenderer& renderer, vk::CommandBuffer& cmd)
{

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 2, renderer.getDrawPipeline().m_draw_descriptor_set_per_scene, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 0, m_draw_descriptor_set_per_model, {});
	for (auto i : m_resource->m_material_index)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 1, m_draw_descriptor_set_per_mesh[i], {});
 		cmd.bindVertexBuffers(0, { m_resource->mMesh.m_vertex_buffer_ex.getBuffer() }, { m_resource->mMesh.m_vertex_buffer_ex.getBufferInfo().offset });
 		cmd.bindIndexBuffer(m_resource->mMesh.m_index_buffer_ex.getBuffer(), m_resource->mMesh.m_index_buffer_ex.getBufferInfo().offset, m_resource->mMesh.mIndexType);
 		cmd.drawIndexedIndirect(m_resource->mMesh.m_indirect_buffer.getBuffer(), vk::DeviceSize(0), m_resource->mMesh.mIndirectCount, sizeof(cModel::Mesh));
	}
}
