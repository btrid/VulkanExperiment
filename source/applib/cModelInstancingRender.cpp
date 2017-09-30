#include <applib/cModelInstancingRender.h>
#include <applib/cModelInstancingPipeline.h>
#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>


std::vector<MotionTexture> createMotion(std::shared_ptr<btr::Context>& loader, vk::CommandBuffer cmd, const cAnimation& anim)
{

	std::vector<MotionTexture> motion_texture(anim.m_motion.size());
	for (size_t i = 0; i < anim.m_motion.size(); i++)
	{
		{
			motion_texture[i] = MotionTexture::create(loader, cmd, anim.m_motion[i]);
		}
	}

	return motion_texture;

}


void ModelInstancingRender::setup(std::shared_ptr<btr::Context>& context, std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum)
{
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	m_resource = resource;
	m_resource_instancing = std::make_unique<InstancingResource>();
	m_resource_instancing->m_instance_max_num = instanceNum;

	// node info
	auto nodeInfo = model::NodeInfo::createNodeInfo(m_resource->mNodeRoot);
	{
		btr::AllocatedMemory::Descriptor staging_desc;
		staging_desc.size = vector_sizeof(nodeInfo);
		staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_node_info_buffer = context->m_staging_memory.allocateMemory(staging_desc);
		auto& buffer = m_resource_instancing->getBuffer(NODE_INFO);
		buffer = context->m_storage_memory.allocateMemory(staging_desc.size);

		memcpy_s(staging_node_info_buffer.getMappedPtr(), staging_desc.size, nodeInfo.data(), staging_desc.size);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_desc.size);
		copy_info.setSrcOffset(staging_node_info_buffer.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd->copyBuffer(staging_node_info_buffer.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

		auto to_render = buffer.makeMemoryBarrierEx();
		to_render.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_render, {});

	}

	if (!m_resource->mBone.empty())
	{
		// BoneInfo
		{
			btr::AllocatedMemory::Descriptor staging_desc;
			staging_desc.size = m_resource->mBone.size() * sizeof(model::BoneInfo);
			staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			btr::BufferMemory staging_bone_info = context->m_staging_memory.allocateMemory(staging_desc);
			auto* bo = static_cast<model::BoneInfo*>(staging_bone_info.getMappedPtr());
			for (size_t i = 0; i < m_resource->mBone.size(); i++) {
				bo[i].mBoneOffset = m_resource->mBone[i].mOffset;
				bo[i].mNodeIndex = m_resource->mBone[i].mNodeIndex;
			}
			auto& buffer = m_resource_instancing->getBuffer(BONE_INFO);
			buffer = context->m_storage_memory.allocateMemory(m_resource->mBone.size() * sizeof(model::BoneInfo));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_bone_info.getBufferInfo().range);
			copy_info.setSrcOffset(staging_bone_info.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd->copyBuffer(staging_bone_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}

		// BoneTransform
		{
			auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM);
			buffer = context->m_storage_memory.allocateMemory(m_resource->mBone.size() * instanceNum * sizeof(BoneTransformBuffer));
		}
	}

	{
		// staging buffer
		btr::AllocatedMemory::Descriptor desc;
		desc.size = instanceNum * sizeof(glm::mat4) * sGlobal::FRAME_MAX;
		m_resource_instancing->m_world_staging = context->m_staging_memory.allocateMemory(desc);
		desc.size = sizeof(ModelInstancingInfo) * sGlobal::FRAME_MAX;
		m_resource_instancing->m_instancing_info = context->m_staging_memory.allocateMemory(desc);
	}

	// ModelInfo
	{
		btr::AllocatedMemory::Descriptor staging_desc;
		staging_desc.size = sizeof(cModel::ModelInfo);
		staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_model_info = context->m_staging_memory.allocateMemory(staging_desc);

		auto& mi = *static_cast<cModel::ModelInfo*>(staging_model_info.getMappedPtr());
		mi = m_resource->m_model_info;

		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INFO);
		buffer = context->m_storage_memory.allocateMemory(sizeof(cModel::ModelInfo));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_model_info.getBufferInfo().range);
		copy_info.setSrcOffset(staging_model_info.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd->copyBuffer(staging_model_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

	}

	//ModelInstancingInfo
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO);
		buffer = context->m_storage_memory.allocateMemory(sizeof(ModelInstancingInfo));
	}
	//BoneMap
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP);
		buffer = context->m_storage_memory.allocateMemory(instanceNum * sizeof(s32));
	}

	//	NodeLocalTransformBuffer
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_LOCAL_TRANSFORM);
		buffer = context->m_storage_memory.allocateMemory(m_resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(NodeLocalTransformBuffer));
	}


	//	NodeGlobalTransformBuffer
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_GLOBAL_TRANSFORM);
		buffer = context->m_storage_memory.allocateMemory(m_resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(NodeGlobalTransformBuffer));
	}
	// world
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::WORLD);
		buffer = context->m_storage_memory.allocateMemory(instanceNum * sizeof(glm::mat4));
	}

	{
		auto& buffer = m_resource_instancing->m_compute_indirect_buffer;
		buffer = context->m_vertex_memory.allocateMemory(sizeof(glm::ivec3) * 6);

		auto staging_compute = context->m_staging_memory.allocateMemory(sizeof(glm::ivec3) * 6);
		auto* group_ptr = static_cast<glm::ivec3*>(staging_compute.getMappedPtr());
		int32_t local_size_x = 1024;
		// shaderのlocal_size_xと合わせる
		std::vector<glm::ivec3> group =
		{
			glm::ivec3(1, 1, 1),
			glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mNodeNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mNodeNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mBoneNum + local_size_x - 1) / local_size_x, 1, 1),
		};
		memcpy_s(group_ptr, sizeof(glm::ivec3) * 6, group.data(), sizeof(glm::ivec3) * 6);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_compute.getBufferInfo().range);
		copy_info.setSrcOffset(staging_compute.getBufferInfo().offset);
		copy_info.setDstOffset(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().offset);
		cmd->copyBuffer(staging_compute.getBufferInfo().buffer, m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().buffer, copy_info);

		vk::BufferMemoryBarrier dispatch_indirect_barrier;
		dispatch_indirect_barrier.setBuffer(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().buffer);
		dispatch_indirect_barrier.setOffset(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().offset);
		dispatch_indirect_barrier.setSize(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().range);
		dispatch_indirect_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
		dispatch_indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
		cmd->pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eDrawIndirect,
			vk::DependencyFlags(),
			{}, { dispatch_indirect_barrier }, {});
	}

	{
		auto& anim = m_resource->getAnimation();
		m_resource_instancing->m_motion_texture = createMotion(context, cmd.get(), m_resource->getAnimation());

		btr::AllocatedMemory::Descriptor staging_desc;
		staging_desc.size = sizeof(ModelInstancingRender::AnimationInfo) * anim.m_motion.size();
		staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(staging_desc);
		auto* staging_ptr = staging.getMappedPtr<ModelInstancingRender::AnimationInfo>();
		for (size_t i = 0; i < anim.m_motion.size(); i++)
		{
			ModelInstancingRender::AnimationInfo& animation = staging_ptr[i];
			animation.duration_ = (float)anim.m_motion[i]->m_duration;
			animation.ticksPerSecond_ = (float)anim.m_motion[i]->m_ticks_per_second;
		}
		// AnimeInfo
		{
			auto& buffer = m_resource_instancing->getBuffer(ANIMATION_INFO);
			btr::AllocatedMemory::Descriptor arg;
			arg.size = sizeof(ModelInstancingRender::AnimationInfo) * anim.m_motion.size();
			buffer = context->m_storage_memory.allocateMemory(arg);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd->copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier barrier;
			barrier.setBuffer(buffer.getBufferInfo().buffer);
			barrier.setOffset(buffer.getBufferInfo().offset);
			barrier.setSize(buffer.getBufferInfo().range);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(),
				{}, { barrier }, {});
		}
		// PlayingAnimation
		{
			btr::AllocatedMemory::Descriptor staging_desc;
			staging_desc.size = instanceNum * sizeof(PlayingAnimation);
			staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_playing_animation = context->m_staging_memory.allocateMemory(staging_desc);

			auto* pa = static_cast<PlayingAnimation*>(staging_playing_animation.getMappedPtr());
			for (int i = 0; i < instanceNum; i++)
			{
				pa[i].playingAnimationNo = 0;
				pa[i].isLoop = true;
				pa[i].time = (float)(std::rand() % 200);
				pa[i].currentMotionInfoIndex = 0;
			}

			auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::PLAYING_ANIMATION);
			buffer = context->m_storage_memory.allocateMemory(instanceNum * sizeof(PlayingAnimation));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_playing_animation.getBufferInfo().range);
			copy_info.setSrcOffset(staging_playing_animation.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd->copyBuffer(staging_playing_animation.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}

	}


}

void ModelInstancingRender::setup(cModelInstancingRenderer&  renderer)
{
	// setup draw
	{
		auto& gpu = sGlobal::Order().getGPU(0);
		auto& device = gpu.getDevice();
		auto& pipeline = renderer.getComputePipeline();

		{
			// meshごとの更新
			vk::DescriptorSetLayout layouts[] = {
				pipeline.m_descriptor_set_layout[cModelInstancingPipeline::DESCRIPTOR_SET_LAYOUT_MODEL].get(),
				pipeline.m_descriptor_set_layout[cModelInstancingPipeline::DESCRIPTOR_SET_LAYOUT_ANIMATION].get(),
			};
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = pipeline.m_descriptor_pool.get();
			allocInfo.descriptorSetCount = array_length(layouts);
			allocInfo.pSetLayouts = layouts;
			m_descriptor_set = device->allocateDescriptorSetsUnique(allocInfo);

			// Model
			{
				std::vector<vk::DescriptorBufferInfo> storages =
				{
					m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
					m_resource_instancing->getBuffer(MODEL_INSTANCING_INFO).getBufferInfo(),
					m_resource_instancing->getBuffer(BONE_TRANSFORM).getBufferInfo(),
				};

				vk::DescriptorImageInfo white_image(DrawHelper::Order().getWhiteTexture().m_sampler.get(), DrawHelper::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
				std::vector<vk::DescriptorImageInfo> albedo_images(16, white_image);
				for (size_t i = 0; i < m_resource->m_mesh.size(); i++)
				{
					auto& material = m_resource->m_material[m_resource->m_mesh[i].m_material_index];
					albedo_images[m_resource->m_mesh[i].m_material_index] = vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
				}

				std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount((uint32_t)storages.size())
					.setPBufferInfo(storages.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_MODEL].get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(albedo_images.size())
					.setPImageInfo(albedo_images.data())
					.setDstBinding(5)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_MODEL].get()),
				};
				device->updateDescriptorSets(drawWriteDescriptorSets, {});
			}
			// AnimationUpdate
			{
				std::vector<vk::DescriptorBufferInfo> storages =
				{
					m_resource_instancing->getBuffer(ANIMATION_INFO).getBufferInfo(),
					m_resource_instancing->getBuffer(PLAYING_ANIMATION).getBufferInfo(),
					m_resource_instancing->getBuffer(NODE_INFO).getBufferInfo(),
					m_resource_instancing->getBuffer(BONE_INFO).getBufferInfo(),
					m_resource_instancing->getBuffer(NODE_LOCAL_TRANSFORM).getBufferInfo(),
					m_resource_instancing->getBuffer(NODE_GLOBAL_TRANSFORM).getBufferInfo(),
					m_resource_instancing->getBuffer(WORLD).getBufferInfo(),
					m_resource_instancing->getBuffer(BONE_MAP).getBufferInfo(),
					m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo(),
				};

				std::vector<vk::DescriptorImageInfo> images =
				{
					vk::DescriptorImageInfo()
					.setImageView(m_resource_instancing->m_motion_texture[0].getImageView())
					.setSampler(m_resource_instancing->m_motion_texture[0].m_resource->m_sampler.get())
					.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				};
				std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount((uint32_t)storages.size())
					.setPBufferInfo(storages.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_ANIMATION].get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(images.size())
					.setPImageInfo(images.data())
					.setDstBinding(32)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_ANIMATION].get()),
				};
				device->updateDescriptorSets(drawWriteDescriptorSets, {});
			}
		}
	}
}

void ModelInstancingRender::addModel(const InstanceResource* data, uint32_t num)
{
	auto cpu = sGlobal::Order().getCPUFrame();
	auto index = m_instance_count[cpu].fetch_add(num);
	auto& staging = m_resource_instancing->m_world_staging;
	auto* world_ptr = static_cast<glm::mat4*>(staging.getMappedPtr()) + m_resource_instancing->m_instance_max_num*cpu;
	for (uint32_t i = 0; i < num; i++)
	{
		world_ptr[index + i] = data[i].m_world;
	}

}

void ModelInstancingRender::execute(cModelInstancingRenderer& renderer, vk::CommandBuffer& cmd)
{
	// bufferの更新
	{
		auto gpu = sGlobal::Order().getGPUFrame();
		int32_t model_count = m_instance_count[gpu];
		if (model_count == 0)
		{
			// やることない
			return;
		}
		// world
		m_instance_count[gpu] = 0;
		{
			auto& staging = m_resource_instancing->m_world_staging;
			auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::WORLD);

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
			to_copy_barrier.setSize(buffer.getBufferInfo().range);
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			vk::BufferCopy copy_info;
			copy_info.setSize(model_count * sizeof(glm::mat4));
			copy_info.setSrcOffset(staging.getBufferInfo().offset + sizeof(glm::mat4) * m_resource_instancing->m_instance_max_num* sGlobal::Order().getCurrentFrame());
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
			to_shader_read_barrier.setSize(copy_info.size);
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}
		{
			// instance num
			auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO);
			auto& staging = m_resource_instancing->m_instancing_info;
			auto* model_info_ptr = staging.getMappedPtr<ModelInstancingInfo>(sGlobal::Order().getCurrentFrame());
			model_info_ptr->mInstanceAliveNum = model_count;
			model_info_ptr->mInstanceMaxNum = m_resource_instancing->m_instance_max_num;
			model_info_ptr->mInstanceNum = 0;

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
			to_copy_barrier.setSize(buffer.getBufferInfo().range);
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			vk::BufferCopy copy_info;
			copy_info.setSize(sizeof(ModelInstancingInfo));
			copy_info.setSrcOffset(staging.getBufferInfo().offset + sizeof(ModelInstancingInfo) * sGlobal::Order().getCurrentFrame());
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
			to_shader_read_barrier.setSize(sizeof(ModelInstancingInfo));
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
		.setBuffer(m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer)
		.setSize(m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().range)
		.setOffset(m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset),
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().buffer)
		.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().range)
		.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().offset)
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_clear_barrier, {});

	auto& pipeline = renderer.getComputePipeline();

	for (size_t i = 0; i < pipeline.m_pipeline.size(); i++)
	{

		if (i == cModelInstancingPipeline::PIPELINE_COMPUTE_CULLING)
		{
			vk::BufferMemoryBarrier barrier = m_resource->m_mesh_resource.m_indirect_buffer_ex.makeMemoryBarrierEx();
			barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
		}
		if (i == cModelInstancingPipeline::PIPELINE_COMPUTE_MOTION_UPDATE)
		{
			// 
			vk::BufferMemoryBarrier barrier = m_resource_instancing->getBuffer(ModelStorageBuffer::PLAYING_ANIMATION).makeMemoryBarrierEx();
			barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
		}

		if (i == cModelInstancingPipeline::PIPELINE_COMPUTE_BONE_TRANSFORM)
		{
			// 
			vk::BufferMemoryBarrier barrier = m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_LOCAL_TRANSFORM).makeMemoryBarrierEx();
			barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
					vk::DependencyFlags(), {}, barrier, {});
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline[i].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[cModelInstancingPipeline::PIPELINE_LAYOUT_COMPUTE].get(), 0, m_descriptor_set[DESCRIPTOR_SET_MODEL].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[cModelInstancingPipeline::PIPELINE_LAYOUT_COMPUTE].get(), 1, m_descriptor_set[DESCRIPTOR_SET_ANIMATION].get(), {});
		cmd.dispatchIndirect(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().buffer, m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().offset + i * 12);

	}
	vk::BufferMemoryBarrier to_draw_barrier = m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).makeMemoryBarrierEx();
	to_draw_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
	to_draw_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, to_draw_barrier, {});

}

void ModelInstancingRender::draw(cModelInstancingRenderer& renderer, vk::CommandBuffer& cmd)
{
	auto& pipeline = renderer.getComputePipeline();
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.m_graphics_pipeline.get());
	vk::ArrayProxy<const vk::DescriptorSet> sets = {
		m_descriptor_set[DESCRIPTOR_SET_MODEL].get(),
		sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
		pipeline.m_descriptor_set_light.get(),
	};
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelInstancingPipeline::PIPELINE_LAYOUT_RENDER].get(), 0, sets, {});
 	cmd.bindVertexBuffers(0, { m_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { m_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
 	cmd.bindIndexBuffer(m_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, m_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, m_resource->m_mesh_resource.mIndexType);
 	cmd.drawIndexedIndirect(m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, m_resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));
}
