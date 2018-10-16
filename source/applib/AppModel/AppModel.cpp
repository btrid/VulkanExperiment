#include <applib/AppModel/AppModel.h>
#include <applib/DrawHelper.h>
#include <applib/GraphicsResource.h>

AppModel::AppModel(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<AppModelContext>& appmodel_context, const std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum)
{
	assert(!resource->mBone.empty());
	m_resource = resource;
	m_instance_max_num = instanceNum;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	// node info
	{
		auto node_info = NodeInfo::createNodeInfo(resource->mNodeRoot);
		btr::BufferMemoryDescriptorEx<NodeInfo> desc;
		desc.element_num = node_info.size();
		b_node_info = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);

		memcpy_s(staging.getMappedPtr(), staging.getInfo().range, node_info.data(), vector_sizeof(node_info));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(b_node_info.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, b_node_info.getInfo().buffer, copy_info);

		auto to_render = b_node_info.makeMemoryBarrier();
		to_render.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_render, {});

	}

	{
		// BoneInfo
		auto bone_info = BoneInfo::createBoneInfo(resource->mBone);
		btr::BufferMemoryDescriptorEx<BoneInfo> desc;
		desc.element_num = bone_info.size();

		auto& buffer = b_bone_info;
		buffer = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);

		memcpy_s(staging.getMappedPtr(), vector_sizeof(bone_info), bone_info.data(), vector_sizeof(bone_info));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(buffer.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);
	}

	b_model_info = context->m_storage_memory.allocateMemory<cModel::ModelInfo>({ 1, {} });
	b_bone_transform = context->m_storage_memory.allocateMemory<mat4>({ resource->mBone.size() * instanceNum, {} });
	b_node_transform = context->m_storage_memory.allocateMemory<mat4>({ resource->mNodeRoot.mNodeList.size() * instanceNum, {} });
	b_world = context->m_storage_memory.allocateMemory<mat4>({ instanceNum, {} });
	b_model_instancing_info = context->m_storage_memory.allocateMemory<ModelInstancingInfo>({ 1, {} });
	b_instance_map = context->m_storage_memory.allocateMemory<uint32_t>({ instanceNum, {} });

	// ModelInfo
	{

		auto staging = context->m_staging_memory.allocateMemory<cModel::ModelInfo>({1, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });

		auto& mi = *static_cast<cModel::ModelInfo*>(staging.getMappedPtr());
		mi = resource->m_model_info;

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(b_model_info.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, b_model_info.getInfo().buffer, copy_info);

	}
	// draw indirect
	{
		btr::BufferMemoryDescriptorEx<cModel::Mesh> desc;
		desc.element_num = resource->m_mesh.size();

		auto& buffer = b_draw_indirect;
		buffer = context->m_vertex_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);

		memcpy_s(staging.getMappedPtr(), staging.getInfo().range, resource->m_mesh.data(), staging.getInfo().range);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(buffer.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);

	}

	{

		auto& buffer = b_animation_indirect;

		btr::BufferMemoryDescriptorEx<uvec3> desc;
		desc.element_num = 6;
		buffer = context->m_vertex_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		auto* group_ptr = static_cast<glm::uvec3*>(staging.getMappedPtr());
		int32_t local_size_x = 1024;
		uvec3 local_size(1024, 1, 1);
		auto nc = (local_size_x / resource->m_model_info.mNodeNum) * resource->m_model_info.mNodeNum;
		auto node_count = instanceNum * resource->m_model_info.mNodeNum / nc + 1; // @todo 正しい計算 +1は無駄な時ある？
		uvec3 group[] =
		{
			uvec3(1, 1, 1),
			app::calcDipatchGroups(uvec3(instanceNum, 1, 1), local_size),
			app::calcDipatchGroups(uvec3(instanceNum*resource->m_model_info.mNodeNum, 1, 1), local_size),
			uvec3(1, node_count, 1),
			app::calcDipatchGroups(uvec3(instanceNum, 1, 1), local_size),
			app::calcDipatchGroups(uvec3(instanceNum*resource->m_model_info.mBoneNum, 1, 1), local_size),
		};
		memcpy_s(group_ptr, sizeof(group), group, sizeof(group));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(b_animation_indirect.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, b_animation_indirect.getInfo().buffer, copy_info);

		vk::BufferMemoryBarrier dispatch_barrier = b_animation_indirect.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eIndirectCommandRead);
		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eDrawIndirect,
			vk::DependencyFlags(),
			{}, { dispatch_barrier }, {});
	}

	{
		auto& anim = resource->getAnimation();
		m_motion_texture = MotionTexture::createMotion(context, cmd, resource->getAnimation());

		btr::BufferMemoryDescriptorEx<AnimationInfo> desc;
		desc.element_num = anim.m_motion.size();

		auto& buffer = b_animation_info;
		buffer = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		auto* staging_ptr = staging.getMappedPtr();
		for (size_t i = 0; i < anim.m_motion.size(); i++)
		{
			auto& animation = staging_ptr[i];
			animation.duration_ = (float)anim.m_motion[i]->m_duration;
			animation.ticksPerSecond_ = (float)anim.m_motion[i]->m_ticks_per_second;
		}

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(buffer.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);

		vk::BufferMemoryBarrier barrier;
		barrier.setBuffer(buffer.getInfo().buffer);
		barrier.setOffset(buffer.getInfo().offset);
		barrier.setSize(buffer.getInfo().range);
		barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			{}, { barrier }, {});
	}
	// PlayingAnimation
	{
		btr::BufferMemoryDescriptorEx<AnimationWorker> desc;
		desc.element_num = instanceNum;

		auto& buffer = b_animation_work;
		buffer = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);

		auto* pa = staging.getMappedPtr();
		for (int i = 0; i < instanceNum; i++)
		{
			pa[i].playingAnimationNo = 0;
			pa[i].isLoop = true;
			pa[i].time = (float)(std::rand() % 200);
			pa[i]._p2 = 0;
		}


		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(buffer.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);
	}

	// material index
	{
		btr::BufferMemoryDescriptorEx<uint32_t> desc;
		desc.element_num = resource->m_mesh.size();
		b_material_index = context->m_storage_memory.allocateMemory(desc);
		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);

		std::vector<uint32_t> material_index(resource->m_mesh.size());
		for (size_t i = 0; i < material_index.size(); i++)
		{
			staging.getMappedPtr()[i] = resource->m_mesh[i].m_material_index;
		}


		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(b_material_index.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, b_material_index.getInfo().buffer, copy_info);

	}

	// material
	{
		btr::BufferMemoryDescriptorEx<MaterialBuffer> desc;
		desc.element_num = resource->m_material.size();
		b_material = context->m_storage_memory.allocateMemory(desc);
		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		auto* mb = static_cast<MaterialBuffer*>(staging.getMappedPtr());
		for (size_t i = 0; i < resource->m_material.size(); i++)
		{
			mb[i].mAmbient = resource->m_material[i].mAmbient;
			mb[i].mDiffuse = resource->m_material[i].mDiffuse;
			mb[i].mEmissive = resource->m_material[i].mEmissive;
			mb[i].mSpecular = resource->m_material[i].mSpecular;
			mb[i].mShininess = resource->m_material[i].mShininess;
		}


		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getInfo().range);
		copy_info.setSrcOffset(staging.getInfo().offset);
		copy_info.setDstOffset(b_material.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, b_material.getInfo().buffer, copy_info);
	}

	// todo 結構適当
	m_albedo_texture.resize(resource->m_material.size() * 1);
	for (size_t i = 0; i < resource->m_material.size(); i++)
	{
		auto& m = resource->m_material[i];
		m_albedo_texture[i * 1 + 0] = m.mDiffuseTex.isReady() ? m.mDiffuseTex : ResourceTexture();
	}

	m_render.m_vertex_buffer = resource->m_mesh_resource.m_vertex_buffer;
	m_render.m_index_buffer = resource->m_mesh_resource.m_index_buffer;
	m_render.m_index_type = resource->m_mesh_resource.mIndexType;
	m_render.m_indirect_buffer = b_draw_indirect.getInfo();
	m_render.m_indirect_count = resource->m_mesh_resource.mIndirectCount;
	m_render.m_indirect_stride = sizeof(cModel::Mesh);

	// initialize
	{
		{
			ModelInstancingInfo info;
			info.mInstanceAliveNum = m_instance_max_num;
			info.mInstanceMaxNum = m_instance_max_num;
			info.mInstanceNum = 0;

			btr::BufferMemoryDescriptorEx<ModelInstancingInfo> desc;
			desc.element_num = 1;
			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			*staging.getMappedPtr() = info;

			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(b_model_instancing_info.getInfo().offset);
			copy.setSize(staging.getInfo().range);
			cmd.copyBuffer(staging.getInfo().buffer, b_model_instancing_info.getInfo().buffer, copy);
		}

		{
			btr::BufferMemoryDescriptorEx<mat4> desc;
			desc.element_num = m_instance_max_num;
			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			for (uint32_t i = 0; i < m_instance_max_num; i++)
			{
				*staging.getMappedPtr(i) = glm::scale(glm::translate(glm::ballRand(700.f)), vec3(.2f));
			}

			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(b_world.getInfo().offset);
			copy.setSize(staging.getInfo().range);
			cmd.copyBuffer(staging.getInfo().buffer, b_world.getInfo().buffer, copy);
		}
	}

	// descriptor_set
	{
		vk::DescriptorSetLayout layouts[] =
		{
			appmodel_context->getLayout(AppModelContext::DescriptorLayout_Model),
			appmodel_context->getLayout(AppModelContext::DescriptorLayout_Render),
			appmodel_context->getLayout(AppModelContext::DescriptorLayout_Update),
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(context->m_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		auto descriptor = context->m_device->allocateDescriptorSetsUnique(descriptor_set_alloc_info);
		m_descriptor_set[0] = std::move(descriptor[0]);
		m_descriptor_set[1] = std::move(descriptor[1]);
		m_descriptor_set[2] = std::move(descriptor[2]);

		vk::DescriptorBufferInfo common_storages[] = {
			b_model_info.getInfo(),
			b_model_instancing_info.getInfo(),
			b_bone_transform.getInfo(),
		};
		vk::DescriptorBufferInfo render_storages[] = {
			b_material_index.getInfo(),
			b_material.getInfo(),
		};

		vk::DescriptorBufferInfo execute_storages[] = {
			b_animation_info.getInfo(),
			b_animation_work.getInfo(),
			b_node_info.getInfo(),
			b_bone_info.getInfo(),
			b_node_transform.getInfo(),
			b_world.getInfo(),
			b_instance_map.getInfo(),
			b_draw_indirect.getInfo(),
		};
		std::array<vk::DescriptorImageInfo, AppModelContext::DESCRIPTOR_ALBEDO_TEXTURE_NUM> albedo_images;
		albedo_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		for (size_t i = 0; i < m_albedo_texture.size(); i++)
		{
			const auto& tex = m_albedo_texture[i];
			if (tex.isReady()) {
				albedo_images[i] = vk::DescriptorImageInfo(tex.getSampler(), tex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
			}
		}
		std::vector<vk::DescriptorImageInfo> animation_images =
		{
			vk::DescriptorImageInfo(m_motion_texture[0].getSampler(),m_motion_texture[0].getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};

		std::vector<vk::WriteDescriptorSet> write =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(common_storages))
			.setPBufferInfo(common_storages)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set[DescriptorSet_Model].get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(render_storages))
			.setPBufferInfo(render_storages)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set[DescriptorSet_Render].get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(albedo_images.size())
			.setPImageInfo(albedo_images.data())
			.setDstBinding(2)
			.setDstSet(m_descriptor_set[DescriptorSet_Render].get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(animation_images.size())
			.setPImageInfo(animation_images.data())
			.setDstBinding(0)
			.setDstSet(m_descriptor_set[DescriptorSet_Update].get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(execute_storages))
			.setPBufferInfo(execute_storages)
			.setDstBinding(1)
			.setDstSet(m_descriptor_set[DescriptorSet_Update].get()),
		};
		context->m_device->updateDescriptorSets(write, {});

	}
}
