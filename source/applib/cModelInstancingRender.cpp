#include <applib/cModelInstancingRender.h>
#include <applib/cModelInstancingPipeline.h>
#include <applib/cModelPipeline.h>
#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>

void ModelInstancingRender::setup(std::shared_ptr<btr::Context>& context, std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum)
{
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	m_material = std::make_shared<DefaultMaterialModule>(context, resource);

	m_resource = resource;
	m_instancing = std::make_shared<InstancingResource>();
	m_instancing->m_instance_max_num = instanceNum;

	// node info
	auto nodeInfo = model::NodeInfo::createNodeInfo(m_resource->mNodeRoot);
	{
		btr::AllocatedMemory::Descriptor staging_desc;
		staging_desc.size = vector_sizeof(nodeInfo);
		staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_node_info_buffer = context->m_staging_memory.allocateMemory(staging_desc);
		auto& buffer = m_instancing->getBuffer(NODE_INFO);
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
			auto& buffer = m_instancing->getBuffer(BONE_INFO);
			buffer = context->m_storage_memory.allocateMemory(m_resource->mBone.size() * sizeof(model::BoneInfo));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_bone_info.getBufferInfo().range);
			copy_info.setSrcOffset(staging_bone_info.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd->copyBuffer(staging_bone_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}

		// BoneTransform
		{
			auto& buffer = m_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM);
			buffer = context->m_storage_memory.allocateMemory(m_resource->mBone.size() * instanceNum * sizeof(mat4));
		}
	}


	// ModelInfo
	{
		btr::AllocatedMemory::Descriptor staging_desc;
		staging_desc.size = sizeof(cModel::ModelInfo);
		staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_model_info = context->m_staging_memory.allocateMemory(staging_desc);

		auto& mi = *static_cast<cModel::ModelInfo*>(staging_model_info.getMappedPtr());
		mi = m_resource->m_model_info;

		auto& buffer = m_instancing->getBuffer(ModelStorageBuffer::MODEL_INFO);
		buffer = context->m_storage_memory.allocateMemory(sizeof(cModel::ModelInfo));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_model_info.getBufferInfo().range);
		copy_info.setSrcOffset(staging_model_info.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd->copyBuffer(staging_model_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

	}

	//ModelInstancingInfo
	{
		btr::UpdateBufferDescriptor desc;
		desc.device_memory = context->m_storage_memory;
		desc.staging_memory = context->m_staging_memory;
		desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
		desc.element_num = 1;
		m_instancing->m_instancing_info_buffer.setup(desc);
	}
	// world
	{
		btr::UpdateBufferDescriptor desc;
		desc.device_memory = context->m_storage_memory;
		desc.staging_memory = context->m_staging_memory;
		desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
		desc.element_num = instanceNum;
		m_instancing->m_world_buffer.setup(desc);
	}

	//BoneMap
	{
		auto& buffer = m_instancing->getBuffer(ModelStorageBuffer::BONE_MAP);
		buffer = context->m_storage_memory.allocateMemory(instanceNum * sizeof(s32));
	}
	// draw indirect
	{
		btr::AllocatedMemory::Descriptor desc;
		desc.size = sizeof(cModel::Mesh)*resource->m_mesh.size();

		auto& buffer = m_instancing->getBuffer(ModelStorageBuffer::DRAW_INDIRECT);
		buffer = context->m_vertex_memory.allocateMemory(desc);

		desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);

		memcpy_s(staging.getMappedPtr(), desc.size, resource->m_mesh.data(), desc.size);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getBufferInfo().range);
		copy_info.setSrcOffset(staging.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd->copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

	}

	{
		auto& buffer = m_instancing->getBuffer(ModelStorageBuffer::NODE_TRANSFORM);
		buffer = context->m_storage_memory.allocateMemory(m_resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(mat4));
	}


	{
		auto& buffer = m_instancing->m_compute_indirect_buffer;
		buffer = context->m_vertex_memory.allocateMemory(sizeof(glm::ivec3) * 6);

		auto staging = context->m_staging_memory.allocateMemory(sizeof(glm::ivec3) * 6);
		auto* group_ptr = static_cast<glm::ivec3*>(staging.getMappedPtr());
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
		copy_info.setSize(staging.getBufferInfo().range);
		copy_info.setSrcOffset(staging.getBufferInfo().offset);
		copy_info.setDstOffset(m_instancing->m_compute_indirect_buffer.getBufferInfo().offset);
		cmd->copyBuffer(staging.getBufferInfo().buffer, m_instancing->m_compute_indirect_buffer.getBufferInfo().buffer, copy_info);

		vk::BufferMemoryBarrier dispatch_indirect_barrier;
		dispatch_indirect_barrier.setBuffer(m_instancing->m_compute_indirect_buffer.getBufferInfo().buffer);
		dispatch_indirect_barrier.setOffset(m_instancing->m_compute_indirect_buffer.getBufferInfo().offset);
		dispatch_indirect_barrier.setSize(m_instancing->m_compute_indirect_buffer.getBufferInfo().range);
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
		m_instancing->m_motion_texture = MotionTexture::createMotion(context, cmd.get(), m_resource->getAnimation());

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
			auto& buffer = m_instancing->getBuffer(ANIMATION_INFO);
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

			auto& buffer = m_instancing->getBuffer(ModelStorageBuffer::PLAYING_ANIMATION);
			buffer = context->m_storage_memory.allocateMemory(instanceNum * sizeof(PlayingAnimation));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_playing_animation.getBufferInfo().range);
			copy_info.setSrcOffset(staging_playing_animation.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd->copyBuffer(staging_playing_animation.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}
	}
}

void ModelInstancingRender::setup(const std::shared_ptr<btr::Context>& context, cModelInstancingPipeline& pipeline)
{
	// setup draw
	{
		auto& device = context->m_device;

		{
			// meshごとの更新
			m_model_descriptor_set = pipeline.m_model_descriptor->allocateDescriptorSet(context);
			pipeline.m_model_descriptor->updateMaterial(context, m_model_descriptor_set.get(), m_material);
			pipeline.m_model_descriptor->updateInstancing(context, m_model_descriptor_set.get(), m_instancing);
			pipeline.m_model_descriptor->updateAnimation(context, m_model_descriptor_set.get(), m_instancing);

			m_animation_descriptor_set = pipeline.m_animation_descriptor->allocateDescriptorSet(context);
			pipeline.m_animation_descriptor->updateAnimation(context, m_animation_descriptor_set.get(), m_instancing);
		}
	}
}

void ModelInstancingRender::addModel(const InstanceResource* data, uint32_t num)
{
	auto frame = sGlobal::Order().getCPUFrame();
	auto index = m_instance_count[frame].fetch_add(num);
	auto* staging = m_instancing->m_world_buffer.mapSubBuffer(frame, index);
	for (uint32_t i = 0; i < num; i++)
	{
		staging[i] = data[i].m_world;
	}

}

void ModelInstancingRender::execute(cModelInstancingPipeline& pipeline, vk::CommandBuffer& cmd)
{
	// bufferの更新
	{
		auto frame = sGlobal::Order().getGPUFrame();
		int32_t model_count = m_instance_count[frame].exchange(0);
		if (model_count == 0)
		{
			// やることない
			return;
		}
		// world
		{
			auto& world = m_instancing->m_world_buffer;
			world.flushSubBuffer(model_count, 0, frame);
			auto& buffer = world.getBufferMemory();

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
			to_copy_barrier.setSize(buffer.getBufferInfo().range);
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			vk::BufferCopy copy_info = world.update(frame);
			cmd.copyBuffer(world.getStagingBufferInfo().buffer, world.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
			to_shader_read_barrier.setSize(copy_info.size);
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}
		{
			auto& instancing = m_instancing->m_instancing_info_buffer;
			ModelInstancingInfo info;
			info.mInstanceAliveNum = model_count;
			info.mInstanceMaxNum = m_instancing->m_instance_max_num;
			info.mInstanceNum = 0;
			instancing.subupdate(&info, 1, 0, frame);

			auto& buffer = instancing.getBufferMemory();
			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
			to_copy_barrier.setSize(buffer.getBufferInfo().range);
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			vk::BufferCopy copy_info = instancing.update(frame);
			cmd.copyBuffer(instancing.getStagingBufferInfo().buffer, instancing.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
			to_shader_read_barrier.setSize(buffer.getBufferInfo().range);
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
		.setBuffer(m_instancing->getDrawIndirect().buffer)
		.setSize(m_instancing->getDrawIndirect().range)
		.setOffset(m_instancing->getDrawIndirect().offset),
		vk::BufferMemoryBarrier()
		.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
		.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
		.setBuffer(m_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().buffer)
		.setSize(m_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().range)
		.setOffset(m_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().offset)
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_clear_barrier, {});

	for (size_t i = 0; i < pipeline.m_pipeline.size(); i++)
	{

		if (i == cModelInstancingPipeline::PIPELINE_COMPUTE_CULLING)
		{
			vk::BufferMemoryBarrier barrier = m_instancing->getBuffer(ModelStorageBuffer::DRAW_INDIRECT).makeMemoryBarrierEx();
			barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
		}
		if (i == cModelInstancingPipeline::PIPELINE_COMPUTE_MOTION_UPDATE)
		{
			// 
			vk::BufferMemoryBarrier barrier = m_instancing->getBuffer(ModelStorageBuffer::PLAYING_ANIMATION).makeMemoryBarrierEx();
			barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
		}

		if (i == cModelInstancingPipeline::PIPELINE_COMPUTE_BONE_TRANSFORM)
		{
			// 
			vk::BufferMemoryBarrier barrier = m_instancing->getBuffer(ModelStorageBuffer::NODE_TRANSFORM).makeMemoryBarrierEx();
			barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
					vk::DependencyFlags(), {}, barrier, {});
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline[i].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[cModelInstancingPipeline::PIPELINE_LAYOUT_COMPUTE].get(), 0, m_model_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[cModelInstancingPipeline::PIPELINE_LAYOUT_COMPUTE].get(), 1, m_animation_descriptor_set.get(), {});
		cmd.dispatchIndirect(m_instancing->m_compute_indirect_buffer.getBufferInfo().buffer, m_instancing->m_compute_indirect_buffer.getBufferInfo().offset + i * 12);

	}
	vk::BufferMemoryBarrier to_draw_barrier = m_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).makeMemoryBarrierEx();
	to_draw_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
	to_draw_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, to_draw_barrier, {});

}

void ModelInstancingRender::draw(cModelInstancingPipeline& pipeline, vk::CommandBuffer& cmd)
{
	vk::ArrayProxy<const vk::DescriptorSet> sets = {
		m_model_descriptor_set.get(),
		sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
		pipeline.getLight()->getDescriptorSet(cFowardPlusPipeline::DESCRIPTOR_SET_LIGHT),
	};
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelInstancingPipeline::PIPELINE_LAYOUT_RENDER].get(), 0, sets, {});

	cmd.bindVertexBuffers(0, { m_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { m_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
 	cmd.bindIndexBuffer(m_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, m_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, m_resource->m_mesh_resource.mIndexType);
 	cmd.drawIndexedIndirect(m_instancing->getDrawIndirect().buffer, m_instancing->getDrawIndirect().offset, m_resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));
}
