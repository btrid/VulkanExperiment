#include <002_model/cModelRender.h>
#include <002_model/cModelPipeline.h>

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
				.setPSetLayouts(&pipeline.m_descriptor_set_layout[cModelDrawPipeline::DESCRIPTOR_SET_LAYOUT_PER_MODEL]);
			m_draw_descriptor_set_per_model = graphics_device->allocateDescriptorSets(alloc_info)[0];

			std::vector<vk::DescriptorBufferInfo> uniformBufferInfo = {
				m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};

			std::vector<vk::DescriptorBufferInfo> storagemBufferInfo = {
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo(),
			};
			std::vector<vk::DescriptorBufferInfo> fStoragemBufferInfo = {
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MATERIAL).getBufferInfo(),
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
			allocInfo.pSetLayouts = &pipeline.m_descriptor_set_layout[cModelDrawPipeline::DESCRIPTOR_SET_LAYOUT_PER_MESH];
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
				m_resource->mMesh.m_indirect_buffer_ex.getBufferInfo(),
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::PLAYING_ANIMATION).getBufferInfo(),
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::PLAYING_ANIMATION).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo(),
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
				.setSampler(m_resource->getAnimation().m_motion_texture.m_resource->m_sampler)
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::WORLD).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::NODE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo(),
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
			std::vector<vk::DescriptorBufferInfo> uniforms = {
				pipeline.m_camera_frustom.getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[4]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::WORLD).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::NODE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getBufferInfo(),
				m_resource->mMesh.m_indirect_buffer_ex.getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
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
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_INFO).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getBufferInfo(),
				m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo(),
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
		// world
		{
			auto& staging = m_resource->m_world_staging_buffer;
			auto* world_ptr = static_cast<glm::mat4*>(staging.getMappedPtr()) + m_resource->m_model_info.mInstanceMaxNum*sGlobal::Order().getCurrentFrame();
			for (auto& m : m_model)
			{
				*world_ptr = m->getInstance()->m_world;
				world_ptr++;
			}
			auto& buffer = m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::WORLD);

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.setBuffer(buffer.getBuffer());
			to_copy_barrier.setOffset(buffer.getOffset());
			to_copy_barrier.setSize(buffer.getSize());
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			vk::BufferCopy copy_info;
			copy_info.setSize(m_model.size() * sizeof(glm::mat4));
			copy_info.setSrcOffset(staging.getOffset() + sizeof(glm::mat4) * m_resource->m_model_info.mInstanceMaxNum* sGlobal::Order().getCurrentFrame());
			copy_info.setDstOffset(buffer.getOffset());
			cmd.copyBuffer(staging.getBuffer(), buffer.getBuffer(), copy_info);

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.setBuffer(buffer.getBuffer());
			to_shader_read_barrier.setOffset(buffer.getOffset());
			to_shader_read_barrier.setSize(copy_info.size);
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}
		{
			// instance num
			auto& buffer = m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO);
			auto& staging = m_resource->m_model_info_staging_buffer;
			auto* model_info_ptr = staging.getMappedPtr<cModelInstancing::ModelInfo>(sGlobal::Order().getCurrentFrame());
			int32_t instance_num = m_model.size();
			model_info_ptr[0] = m_resource->m_model_info;
			model_info_ptr[0].mInstanceAliveNum = instance_num;
			model_info_ptr[0].mInstanceNum = 0;

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.setBuffer(buffer.getBuffer());
			to_copy_barrier.setOffset(buffer.getOffset());
			to_copy_barrier.setSize(sizeof(cModelInstancing::ModelInfo));
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			vk::BufferCopy copy_info;
			copy_info.setSize(sizeof(cModelInstancing::ModelInfo));
			copy_info.setSrcOffset(staging.getOffset() + sizeof(cModelInstancing::ModelInfo) * sGlobal::Order().getCurrentFrame());
			copy_info.setDstOffset(buffer.getOffset());
			cmd.copyBuffer(staging.getBuffer(), buffer.getBuffer(), copy_info);

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.setBuffer(buffer.getBuffer());
			to_shader_read_barrier.setOffset(buffer.getOffset());
			to_shader_read_barrier.setSize(sizeof(cModelInstancing::ModelInfo));
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}

	}

	std::vector<vk::BufferMemoryBarrier> to_clear_barrier =
	{
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(m_resource->mMesh.m_indirect_buffer_ex.getBuffer())
		.setSize(m_resource->mMesh.m_indirect_buffer_ex.getSize())
		.setOffset(m_resource->mMesh.m_indirect_buffer_ex.getOffset()),
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getBuffer())
		.setSize(m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getSize())
		.setOffset(m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getOffset())
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_clear_barrier, {});

	auto& pipeline = renderer.getComputePipeline();
	for (size_t i = 0; i < pipeline.m_pipeline_layout.size(); i++)
	{

		if (i == 4)
		{
			// 
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setBuffer(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBuffer())
				.setOffset(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getOffset())
				.setSize(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getSize()),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setBuffer(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getBuffer())
				.setOffset(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getOffset())
				.setSize(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getSize()),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setBuffer(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getBuffer())
				.setOffset(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getOffset())
				.setSize(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getSize()),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(), {}, barrier, {});

		}
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline[i]);
	 	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[i], 0, m_compute_descriptor_set[i], {});
		cmd.dispatchIndirect(m_resource->m_compute_indirect_buffer.getBuffer(), m_resource->m_compute_indirect_buffer.getBufferInfo().offset + i* 12);


		if (i == 3)
		{ 
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getBuffer())
				.setOffset(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getOffset())
				.setSize(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getSize()),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(), {}, barrier, {});
 		}
		if (i == 4)
		{
			// 
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getBuffer())
				.setOffset(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getOffset())
				.setSize(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::MODEL_INFO).getSize()),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getBuffer())
				.setOffset(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getOffset())
				.setSize(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::BONE_MAP).getSize()),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getBuffer())
				.setOffset(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getOffset())
				.setSize(m_resource->getBuffer(cModelInstancing::cModelInstancing::Resource::ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getSize()),
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
		.setBuffer(m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getBuffer())
		.setOffset(m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getOffset())
		.setSize(m_resource->getBuffer(cModelInstancing::Resource::ModelStorageBuffer::BONE_TRANSFORM).getSize()),
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
		.setBuffer(m_resource->mMesh.m_indirect_buffer_ex.getBuffer())
		.setOffset(m_resource->mMesh.m_indirect_buffer_ex.getOffset())
		.setSize(m_resource->mMesh.m_indirect_buffer_ex.getSize())
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_draw_barrier, {});

}

void ModelRender::draw(cModelRenderer& renderer, vk::CommandBuffer& cmd)
{

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 2, renderer.getDrawPipeline().m_descriptor_set_per_scene, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 0, m_draw_descriptor_set_per_model, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 3, renderer.getDrawPipeline().m_descriptor_light, {});
	for (auto i : m_resource->m_material_index)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 1, m_draw_descriptor_set_per_mesh[i], {});
 		cmd.bindVertexBuffers(0, { m_resource->mMesh.m_vertex_buffer_ex.getBuffer() }, { m_resource->mMesh.m_vertex_buffer_ex.getOffset() });
 		cmd.bindIndexBuffer(m_resource->mMesh.m_index_buffer_ex.getBuffer(), m_resource->mMesh.m_index_buffer_ex.getOffset(), m_resource->mMesh.mIndexType);
 		cmd.drawIndexedIndirect(m_resource->mMesh.m_indirect_buffer_ex.getBuffer(), m_resource->mMesh.m_indirect_buffer_ex.getOffset(), m_resource->mMesh.mIndirectCount, sizeof(cModelInstancing::Mesh));
	}
}
