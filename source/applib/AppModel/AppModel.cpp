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
		b_node_info = context->m_storage_memory.allocateMemory<NodeInfo>(node_info.size());

		auto staging = context->m_staging_memory.allocateMemory<NodeInfo>(node_info.size(), true);
		memcpy_s(staging.getMappedPtr(), staging.getInfo().range, node_info.data(), vector_sizeof(node_info));

		vk::BufferCopy copy_info = vk::BufferCopy().setSize(staging.getInfo().range).setSrcOffset(staging.getInfo().offset).setDstOffset(b_node_info.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, b_node_info.getInfo().buffer, copy_info);

		auto to_render = b_node_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_render, {});
	}

	{
		// BoneInfo
		auto& buffer = b_bone_info;
		auto bone_info = BoneInfo::createBoneInfo(resource->mBone);
		buffer = context->m_storage_memory.allocateMemory<BoneInfo>(bone_info.size());
		auto staging = context->m_staging_memory.allocateMemory<BoneInfo>(bone_info.size(), true);

		memcpy_s(staging.getMappedPtr(), vector_sizeof(bone_info), bone_info.data(), vector_sizeof(bone_info));

		vk::BufferCopy copy_info = vk::BufferCopy().setSize(staging.getInfo().range).setSrcOffset(staging.getInfo().offset).setDstOffset(buffer.getInfo().offset);
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
		cmd.updateBuffer<cModel::ModelInfo>(b_model_info.getInfo().buffer, b_model_info.getInfo().offset, resource->m_model_info);
	}
	// draw indirect
	{
		b_draw_indirect = context->m_vertex_memory.allocateMemory<cModel::Mesh>(resource->m_mesh.size());
		cmd.updateBuffer(b_draw_indirect.getInfo().buffer, b_draw_indirect.getInfo().offset, vector_sizeof(resource->m_mesh), resource->m_mesh.data());
	}

	{

		auto& buffer = b_animation_indirect;

		buffer = context->m_vertex_memory.allocateMemory<uvec3>(6);
		auto staging = context->m_staging_memory.allocateMemory<uvec3>(6, true);

		uvec3 local_size(1024, 1, 1);
		auto nc = (local_size.x / resource->m_model_info.mNodeNum) * resource->m_model_info.mNodeNum;
		auto dispach_count = ((instanceNum * resource->m_model_info.mNodeNum) + (nc - 1)) / nc;
		uvec3 group[] =
		{
			uvec3(1, 1, 1),
			app::calcDipatchGroups(uvec3(instanceNum, 1, 1), local_size),
			app::calcDipatchGroups(uvec3(instanceNum*resource->m_model_info.mNodeNum, 1, 1), local_size),
			uvec3(1, dispach_count, 1),
			app::calcDipatchGroups(uvec3(instanceNum, 1, 1), local_size),
			app::calcDipatchGroups(uvec3(instanceNum*resource->m_model_info.mBoneNum, 1, 1), local_size),
		};
		memcpy_s(staging.getMappedPtr(), sizeof(group), group, sizeof(group));

		vk::BufferCopy copy_info = vk::BufferCopy().setSize(staging.getInfo().range).setSrcOffset(staging.getInfo().offset).setDstOffset(b_animation_indirect.getInfo().offset);
		cmd.copyBuffer(staging.getInfo().buffer, b_animation_indirect.getInfo().buffer, copy_info);

		vk::BufferMemoryBarrier dispatch_barrier = b_animation_indirect.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eIndirectCommandRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { dispatch_barrier }, {});
	}

	{
		auto& anim = resource->getAnimation();
		m_motion_texture = MotionTexture::createMotion(context, cmd, resource->getAnimation());

		b_animation_info = context->m_storage_memory.allocateMemory<AnimationInfo>(anim.m_motion.size());
		std::vector<AnimationInfo> data(anim.m_motion.size());
		for (size_t i = 0; i < anim.m_motion.size(); i++)
		{
			auto& animation = data[i];
			animation.duration_ = (float)anim.m_motion[i]->m_duration;
			animation.ticksPerSecond_ = (float)anim.m_motion[i]->m_ticks_per_second;
			animation.numInfo_ = 0;
			animation.offsetInfo_ = 0;
		}

		cmd.updateBuffer(b_animation_info.getInfo().buffer, b_animation_info.getInfo().offset, vector_sizeof(data), data.data());

		vk::BufferMemoryBarrier barrier = b_animation_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite,vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
	}
	// PlayingAnimation
	{
		b_animation_work = context->m_storage_memory.allocateMemory<AnimationWorker>(instanceNum);

		std::vector<AnimationWorker> worker(instanceNum);
		for (int i = 0; i < instanceNum; i++)
		{
			worker[i].playingAnimationNo = 0;
			worker[i].isLoop = true;
			worker[i].time = (float)(std::rand() % 1000);
			worker[i]._p2 = 0;
		}

		cmd.updateBuffer(b_animation_work.getInfo().buffer, b_animation_work.getInfo().offset, vector_sizeof(worker), worker.data());
	}

	// material index
	{
		b_material_index = context->m_storage_memory.allocateMemory<uint32_t>(resource->m_mesh.size());

		std::vector<uint32_t> material_index(resource->m_mesh.size());
		for (size_t i = 0; i < material_index.size(); i++)
		{
			material_index[i] = resource->m_mesh[i].m_material_index;
		}
		cmd.updateBuffer(b_material_index.getInfo().buffer, b_material_index.getInfo().offset, vector_sizeof(material_index), material_index.data());

	}

	// material
	{
		b_material = context->m_storage_memory.allocateMemory<MaterialBuffer>(resource->m_material.size());
		std::vector<MaterialBuffer> material(resource->m_material.size());
		for (size_t i = 0; i < resource->m_material.size(); i++)
		{
			material[i].mAmbient = resource->m_material[i].mAmbient;
			material[i].mDiffuse = resource->m_material[i].mDiffuse;
			material[i].mEmissive = resource->m_material[i].mEmissive;
			material[i].mSpecular = resource->m_material[i].mSpecular;
			material[i].mShininess = resource->m_material[i].mShininess;
		}
		cmd.updateBuffer(b_material.getInfo().buffer, b_material.getInfo().offset, vector_sizeof(material), material.data());
	}

	// todo 結構適当
	m_render.m_data = resource;
	m_render.m_indirect_buffer = b_draw_indirect.getInfo();

	// initialize
	{
		{
			ModelInstancingInfo info;
			info.mInstanceAliveNum = m_instance_max_num;
			info.mInstanceMaxNum = m_instance_max_num;
			info.mInstanceNum = 0;

			cmd.updateBuffer<ModelInstancingInfo>(b_model_instancing_info.getInfo().buffer, b_model_instancing_info.getInfo().offset, info);
		}

		{
			auto staging = context->m_staging_memory.allocateMemory<mat4>(m_instance_max_num, true);
			for (uint32_t i = 0; i < m_instance_max_num; i++)
			{
				*staging.getMappedPtr(i) = glm::scale(glm::translate(glm::mat4(1.f), glm::ballRand(700.f)), vec3(.2f));
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
		auto descriptor = context->m_device.allocateDescriptorSetsUnique(descriptor_set_alloc_info);
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
		std::array<vk::DescriptorImageInfo, AppModelContext::DESCRIPTOR_ALBEDO_TEXTURE_NUM> normal_images;
		std::array<vk::DescriptorImageInfo, AppModelContext::DESCRIPTOR_ALBEDO_TEXTURE_NUM> emissive_images;
		std::array<vk::DescriptorImageInfo, AppModelContext::DESCRIPTOR_ALBEDO_TEXTURE_NUM> metalness_images;
		std::array<vk::DescriptorImageInfo, AppModelContext::DESCRIPTOR_ALBEDO_TEXTURE_NUM> roughness_images;
		std::array<vk::DescriptorImageInfo, AppModelContext::DESCRIPTOR_ALBEDO_TEXTURE_NUM> occlusion_images;
		albedo_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		normal_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		emissive_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		metalness_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		roughness_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		occlusion_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		for (size_t i = 0; i < resource->m_material.size(); i++)
		{
			auto& m = resource->m_material[i];
			auto& t = m.mTex[cModel::ResourceTextureIndex_Base];
			if (t.isReady()) { albedo_images[i] = vk::DescriptorImageInfo(t.getSampler(), t.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal); }
			t = m.mTex[cModel::ResourceTextureIndex_NormalCamera];
			if (t.isReady()) { normal_images[i] = vk::DescriptorImageInfo(t.getSampler(), t.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal); }
			t = m.mTex[cModel::ResourceTextureIndex_Emissive];
			if (t.isReady()) { emissive_images[i] = vk::DescriptorImageInfo(t.getSampler(), t.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal); }
			t = m.mTex[cModel::ResourceTextureIndex_Metalness];
			if (t.isReady()) { metalness_images[i] = vk::DescriptorImageInfo(t.getSampler(), t.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal); }
			t = m.mTex[cModel::ResourceTextureIndex_DiffuseRoughness];
			if (t.isReady()) { roughness_images[i] = vk::DescriptorImageInfo(t.getSampler(), t.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal); }
			t = m.mTex[cModel::ResourceTextureIndex_AmbientOcclusion];
			if (t.isReady()) { occlusion_images[i] = vk::DescriptorImageInfo(t.getSampler(), t.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal); }
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
			vk::WriteDescriptorSet().setDstSet(m_descriptor_set[DescriptorSet_Render].get()).setDstBinding(10).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(albedo_images.size()).setPImageInfo(albedo_images.data()),
			vk::WriteDescriptorSet().setDstSet(m_descriptor_set[DescriptorSet_Render].get()).setDstBinding(11).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(normal_images.size()).setPImageInfo(normal_images.data()),
			vk::WriteDescriptorSet().setDstSet(m_descriptor_set[DescriptorSet_Render].get()).setDstBinding(12).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(emissive_images.size()).setPImageInfo(emissive_images.data()),
			vk::WriteDescriptorSet().setDstSet(m_descriptor_set[DescriptorSet_Render].get()).setDstBinding(13).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(metalness_images.size()).setPImageInfo(metalness_images.data()),
			vk::WriteDescriptorSet().setDstSet(m_descriptor_set[DescriptorSet_Render].get()).setDstBinding(14).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(roughness_images.size()).setPImageInfo(roughness_images.data()),
			vk::WriteDescriptorSet().setDstSet(m_descriptor_set[DescriptorSet_Render].get()).setDstBinding(15).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(occlusion_images.size()).setPImageInfo(occlusion_images.data()),
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
		context->m_device.updateDescriptorSets(write, {});

	}
}
