#include <002_model/cModelRender.h>
#include <002_model/cModelRenderer.h>

void ModelRender::setup(cModelRenderer&  renderer)
{
	// setup graphics cmdbuffer 
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
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).getBufferInfo(),
			};

			std::vector<vk::DescriptorBufferInfo> storagemBufferInfo = {
//				mPrivate->getBuffer(Private::ModelBuffer::VS_MATERIAL).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> fStoragemBufferInfo = {
				mPrivate->getBuffer(Private::ModelBuffer::MATERIAL).getBufferInfo(),
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
			allocInfo.descriptorSetCount = mPrivate->mMeshNum;
			allocInfo.pSetLayouts = &pipeline.m_descriptor_set_layout[cModelRenderer::cModelDrawPipeline::DESCRIPTOR_SET_LAYOUT_PER_MESH];
			m_draw_descriptor_set_per_mesh = graphics_device->allocateDescriptorSets(allocInfo);
			for (size_t i = 0; i < m_draw_descriptor_set_per_mesh.size(); i++)
			{
				auto& material = mPrivate->m_material[mPrivate->m_material_index[i]];

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

	//vk::CommandBuffer cModel::buildExecuteCmd()
	{
		cDevice compute_device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_COMPUTE];

		auto& pipeline = renderer.getComputePipeline();
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.setDescriptorPool(pipeline.m_descriptor_pool);
		allocInfo.setDescriptorSetCount(pipeline.m_descriptor_set_layout.size());
		allocInfo.setPSetLayouts(pipeline.m_descriptor_set_layout.data());
		m_compute_descriptor_set = compute_device->allocateDescriptorSets(allocInfo);

		vk::WriteDescriptorSet desc;
		// Clear
		{
			std::vector<vk::DescriptorBufferInfo> storages =
			{
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).getBufferInfo(),
				mPrivate->mMesh.m_indirect_buffer.getBufferInfo(),
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
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).getBufferInfo(),
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
				mPrivate->getMotionBuffer(cAnimation::ANIMATION_INFO).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::PLAYING_ANIMATION).getBufferInfo(),
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
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).getBufferInfo(),
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
				mPrivate->getMotionBuffer(cAnimation::ANIMATION_INFO).getBufferInfo(),
				mPrivate->getMotionBuffer(cAnimation::MOTION_INFO).getBufferInfo(),
				mPrivate->getMotionBuffer(cAnimation::MOTION_DATA_TIME).getBufferInfo(),
				mPrivate->getMotionBuffer(cAnimation::MOTION_DATA_SRT).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::PLAYING_ANIMATION).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::MOTION_WORK).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[2]);
			compute_device->updateDescriptorSets(desc, {});
		}

		// NodeTransform
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).getBufferInfo(),
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
				mPrivate->getBuffer(Private::ModelBuffer::WORLD).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo(),
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
				mPrivate->getBuffer(Private::ModelBuffer::WORLD).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::BONE_INFO).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::BONE_MAP).getBufferInfo(),
				mPrivate->mMesh.m_indirect_buffer.getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).getBufferInfo(),
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
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).getBufferInfo(),
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
				mPrivate->getBuffer(Private::ModelBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::BONE_INFO).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::BONE_MAP).getBufferInfo(),
				mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[5]);
			compute_device->updateDescriptorSets(desc, {});
		}
//		compute_device->updateDescriptorSets(compute_write_descriptor, {});

// 		for (size_t i = 0; i < renderer.getComputePipeline().m_layout.size(); i++)
// 		{
// 			m_cmd_compute[1].bindPipeline(vk::PipelineBindPoint::eCompute, renderer.getComputePipeline().m_pipeline[i]);
// 			m_cmd_compute[1].bindDescriptorSets(vk::PipelineBindPoint::eCompute, renderer.getComputePipeline().m_layout[i], 0, std::vector<vk::DescriptorSet>{ m_compute_descriptor_set[i] }, {});
// 			m_cmd_compute[1].dispatch(256, 1, 1);
// 			// 					m_cmd[1].dispatchIndirect(mComputePipeline.mDisptachIndirectInfo.buffer(), vk::DeviceSize(sizeof(glm::ivec3)*i));
// 		}

		std::vector<vk::BufferMemoryBarrier> barrier =
		{
			vk::BufferMemoryBarrier()
			.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
			.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eMemoryRead)
			.setBuffer(mPrivate->mMesh.m_indirect_buffer.getBuffer())
			.setSize(mPrivate->mMesh.m_indirect_buffer.getBufferSize())
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED),
			vk::BufferMemoryBarrier()
			.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eMemoryRead)
			.setBuffer(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBuffer())
			.setSize(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBufferInfo().range)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED),
		};
	}
}
void ModelRender::execute(cModelRenderer& renderer, vk::CommandBuffer& cmd)
{
	std::vector<vk::BufferMemoryBarrier> to_clear_barrier =
	{
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(mPrivate->mMesh.m_indirect_buffer.getBuffer())
		.setSize(mPrivate->mMesh.m_indirect_buffer.getBufferSize()),
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBuffer())
		.setSize(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBufferSize())
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_clear_barrier, {});

	auto& pipeline = renderer.getComputePipeline();
	for (size_t i = 0; i < pipeline.m_pipeline_layout.size(); i++)
	{
	 	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline[i]);
	 	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[i], 0, m_compute_descriptor_set[i], {});
		cmd.dispatchIndirect(mPrivate->m_compute_indirect_buffer.getBuffer(), i * 12);

		if (i == 4) 
		{
			// 
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(mPrivate->getBuffer(cModel::Private::ModelBuffer::MODEL_INFO).getBuffer())
				.setSize(mPrivate->getBuffer(cModel::Private::ModelBuffer::MODEL_INFO).getBufferInfo().range),
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
		.setBuffer(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBuffer())
		.setSize(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).getBufferInfo().range)
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_draw_barrier, {});

}

void ModelRender::draw(cModelRenderer& renderer, vk::CommandBuffer& cmd)
{

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 2, renderer.getDrawPipeline().m_draw_descriptor_set_per_scene, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 0, m_draw_descriptor_set_per_model, {});
	for (auto i : mPrivate->m_material_index)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 1, m_draw_descriptor_set_per_mesh[i], {});
		cmd.bindVertexBuffers(0, { mPrivate->mMesh.m_vertex_buffer.getBuffer() }, { vk::DeviceSize() });
		cmd.bindIndexBuffer(mPrivate->mMesh.m_index_buffer.getBuffer(), vk::DeviceSize(0), mPrivate->mMesh.mIndexType);
		cmd.drawIndexedIndirect(mPrivate->mMesh.m_indirect_buffer.getBuffer(), vk::DeviceSize(0), mPrivate->mMesh.mIndirectCount, sizeof(cModel::Mesh));
	}
}
