#include <002_model/cModelRender.h>
#include <002_model/cModelPipeline.h>

/**
*	モーションのデータを一枚の1DArrayに格納
*/
MotionTexture create(app::Loader* loader, const Motion& motion, const RootNode& root)
{
	uint32_t SIZE = 256;

	vk::ImageCreateInfo image_info;
	image_info.imageType = vk::ImageType::e1D;
	image_info.format = vk::Format::eR16G16B16A16Sfloat;
	image_info.mipLevels = 1;
	image_info.arrayLayers = (uint32_t)root.mNodeList.size() * 3u;
	image_info.samples = vk::SampleCountFlagBits::e1;
	image_info.tiling = vk::ImageTiling::eOptimal;
	image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	image_info.sharingMode = vk::SharingMode::eExclusive;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
	image_info.extent = { SIZE, 1u, 1u };

	auto image_prop = loader->m_device.getGPU().getImageFormatProperties(image_info.format, image_info.imageType, image_info.tiling, image_info.usage, image_info.flags);
	vk::Image image = loader->m_device->createImage(image_info);

	btr::BufferMemory::Descriptor staging_desc;
	staging_desc.size = image_info.arrayLayers * motion.m_data.size() * SIZE * sizeof(glm::u16vec4);
	staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_buffer = loader->m_staging_memory.allocateMemory(staging_desc);
	auto* data = staging_buffer.getMappedPtr<glm::u16vec4>();

	for (size_t i = 0; i < motion.m_data.size(); i++)
	{
		const Motion::NodeMotion& node_motion = motion.m_data[i];
		auto no = node_motion.m_node_index;
		auto* pos = reinterpret_cast<uint64_t*>(data + no * 3 * SIZE);
		auto* quat = reinterpret_cast<uint64_t*>(pos + SIZE);
		auto* scale = reinterpret_cast<uint64_t*>(quat + SIZE);

		float time_max = motion.m_duration;// / anim->mTicksPerSecond;
		float time_per = time_max / SIZE;
		float time = 0.;
		int count = 0;
		for (size_t n = 0; n < node_motion.m_scale.size() - 1;)
		{
			auto& current = node_motion.m_scale[n];
			auto& next = node_motion.m_scale[n + 1];

			if (time >= next.m_time)
			{
				n++;
				continue;
			}
			auto current_value = glm::vec4(current.m_data.x, current.m_data.y, current.m_data.z, 0.f);
			auto next_value = glm::vec4(next.m_data.x, next.m_data.y, next.m_data.z, 0.f);
			auto rate = 1.f - (next.m_time - time) / (next.m_time - current.m_time);
			auto value = glm::lerp(current_value, next_value, rate);
			scale[count] = glm::packHalf4x16(value);
			count++;
			time += time_per;
		}
		assert(count == SIZE);
		time = 0.;
		count = 0;
		for (size_t n = 0; n < node_motion.m_translate.size() - 1;)
		{
			auto& current = node_motion.m_translate[n];
			auto& next = node_motion.m_translate[n + 1];

			if (time >= next.m_time)
			{
				n++;
				continue;
			}
			auto current_value = glm::vec3(current.m_data.x, current.m_data.y, current.m_data.z);
			auto next_value = glm::vec3(next.m_data.x, next.m_data.y, next.m_data.z);
			auto rate = 1.f - (float)(next.m_time - time) / (float)(next.m_time - current.m_time);
			auto value = glm::lerp<float>(current_value, next_value, rate);
			pos[count] = glm::packHalf4x16(glm::vec4(value, 0.f));
			count++;
			time += time_per;
		}
		assert(count == SIZE);
		time = 0.;
		count = 0;
		for (size_t n = 0; n < node_motion.m_rotate.size() - 1;)
		{
			auto& current = node_motion.m_rotate[n];
			auto& next = node_motion.m_rotate[n + 1];

			if (time >= next.m_time)
			{
				n++;
				continue;
			}
			auto current_value = current.m_data;
			auto next_value = next.m_data;
			auto rate = 1.f - (float)(next.m_time - time) / (float)(next.m_time - current.m_time);
			auto value = glm::slerp<float>(current_value, next_value, rate);
			quat[count] = glm::packHalf4x16(glm::vec4(value.x, value.y, value.z, value.w));
			count++;
			time += time_per;
		}
		assert(count == SIZE);
	}

	vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(image);
	vk::MemoryAllocateInfo memory_alloc_info;
	memory_alloc_info.allocationSize = memory_request.size;
	memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::DeviceMemory image_memory = loader->m_device->allocateMemory(memory_alloc_info);
	loader->m_device->bindImageMemory(image, image_memory, 0);

	vk::ImageSubresourceRange subresourceRange;
	subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.layerCount = image_info.arrayLayers;
	subresourceRange.levelCount = 1;

	vk::BufferImageCopy copy;
	copy.bufferOffset = staging_buffer.getBufferInfo().offset;
	copy.imageExtent = { SIZE, 1u, 1u };
	copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = image_info.arrayLayers;
	copy.imageSubresource.mipLevel = 0;

	vk::ImageMemoryBarrier to_copy_barrier;
	to_copy_barrier.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
	to_copy_barrier.image = image;
	to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
	to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	to_copy_barrier.subresourceRange = subresourceRange;

	vk::ImageMemoryBarrier to_shader_read_barrier;
	to_shader_read_barrier.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
	to_shader_read_barrier.image = image;
	to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	to_shader_read_barrier.subresourceRange = subresourceRange;

	loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
	loader->m_cmd.copyBufferToImage(staging_buffer.getBufferInfo().buffer, image, vk::ImageLayout::eTransferDstOptimal, { copy });
	loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });

	MotionTexture tex;
	tex.m_resource = std::make_shared<MotionTexture::Resource>();
	tex.m_resource->m_device = loader->m_device;
	tex.m_resource->m_image = image;

	vk::ImageViewCreateInfo view_info;
	view_info.viewType = vk::ImageViewType::e1DArray;
	view_info.components.r = vk::ComponentSwizzle::eR;
	view_info.components.g = vk::ComponentSwizzle::eG;
	view_info.components.b = vk::ComponentSwizzle::eB;
	view_info.components.a = vk::ComponentSwizzle::eA;
	view_info.flags = vk::ImageViewCreateFlags();
	view_info.format = image_info.format;
	view_info.image = image;
	view_info.subresourceRange = subresourceRange;

	tex.m_resource->m_image_view = loader->m_device->createImageView(view_info);
	tex.m_resource->m_memory = image_memory;

	vk::SamplerCreateInfo sampler_info;
	sampler_info.magFilter = vk::Filter::eNearest;
	sampler_info.minFilter = vk::Filter::eNearest;
	sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;//
	sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.compareOp = vk::CompareOp::eNever;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.f;
	sampler_info.maxAnisotropy = 1.0;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

	tex.m_resource->m_sampler = loader->m_device->createSampler(sampler_info);
	return tex;
}

std::vector<MotionTexture> createMotion(app::Loader* loader, const cAnimation& anim, const RootNode& rootnode)
{

	std::vector<MotionTexture> motion_texture(anim.m_motion.size());
	for (size_t i = 0; i < anim.m_motion.size(); i++)
	{
		{
			motion_texture[i] = create(loader, anim.m_motion[i], rootnode);
		}
	}

	return motion_texture;

}
void createNodeInfoRecurcive(const RootNode& rootnode, const Node& node, std::vector<ModelRender::NodeInfo>& nodeBuffer, int parentIndex)
{
	nodeBuffer.emplace_back();
	auto& n = nodeBuffer.back();
	n.mNodeNo = (s32)nodeBuffer.size() - 1;
	n.mParent = parentIndex;
	n.m_depth = nodeBuffer[parentIndex].m_depth + 1;
	for (size_t i = 0; i < node.mChildren.size(); i++) {
		createNodeInfoRecurcive(rootnode, rootnode.mNodeList[node.mChildren[i]], nodeBuffer, n.mNodeNo);
	}
}

std::vector<ModelRender::NodeInfo> createNodeInfo(const RootNode& rootnode)
{
	std::vector<ModelRender::NodeInfo> nodeBuffer;
	nodeBuffer.reserve(rootnode.mNodeList.size());
	nodeBuffer.emplace_back();
	auto& node = nodeBuffer.back();
	node.mNodeNo = (int32_t)nodeBuffer.size() - 1;
	node.mParent = -1;
	node.m_depth = 0;
	for (size_t i = 0; i < rootnode.mNodeList[0].mChildren.size(); i++) {
		createNodeInfoRecurcive(rootnode, rootnode.mNodeList[rootnode.mNodeList[0].mChildren[i]], nodeBuffer, node.mNodeNo);
	}

	return nodeBuffer;
}


void ModelRender::setup(app::Loader* loader, std::shared_ptr<cModel::Resource> resource, uint32_t instanceNum)
{
	m_resource = resource;
	m_resource_instancing = std::make_unique<InstancingResource>();
	m_resource_instancing->m_instance_max_num = instanceNum;
	// material
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = m_resource->m_material.size() * sizeof(MaterialBuffer);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_material = loader->m_staging_memory.allocateMemory(staging_desc);
		auto* mb = static_cast<MaterialBuffer*>(staging_material.getMappedPtr());
		for (size_t i = 0; i < m_resource->m_material.size(); i++)
		{
			mb[i].mAmbient = m_resource->m_material[i].mAmbient;
			mb[i].mDiffuse = m_resource->m_material[i].mDiffuse;
			mb[i].mEmissive = m_resource->m_material[i].mEmissive;
			mb[i].mSpecular = m_resource->m_material[i].mSpecular;
			mb[i].mShininess = m_resource->m_material[i].mShininess;
		}

		auto& buffer = m_resource_instancing->getBuffer(MATERIAL);
		buffer = loader->m_storage_memory.allocateMemory(m_resource->m_material.size() * sizeof(MaterialBuffer));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_material.getBufferInfo().range);
		copy_info.setSrcOffset(staging_material.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		loader->m_cmd.copyBuffer(staging_material.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

	}
	// material index
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = m_resource->m_mesh.size() * sizeof(uint32_t);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = loader->m_staging_memory.allocateMemory(staging_desc);
		auto* mi = static_cast<uint32_t*>(staging.getMappedPtr());
		for (size_t i = 0; i < m_resource->m_mesh.size(); i++) {
			mi[i] = m_resource->m_mesh[i].m_material_index;
		}

		auto& buffer = m_resource_instancing->getBuffer(MATERIAL_INDEX);
		buffer = loader->m_storage_memory.allocateMemory(staging_desc.size);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging.getBufferInfo().range);
		copy_info.setSrcOffset(staging.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

		auto to_render = buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
		loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput, {}, {}, to_render, {});
	}
	// node info
	auto nodeInfo = createNodeInfo(m_resource->mNodeRoot);
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = vector_sizeof(nodeInfo);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_node_info_buffer = loader->m_staging_memory.allocateMemory(staging_desc);
		auto& buffer = m_resource_instancing->getBuffer(NODE_INFO);
		buffer = loader->m_storage_memory.allocateMemory(staging_desc.size);

		memcpy_s(staging_node_info_buffer.getMappedPtr(), staging_desc.size, nodeInfo.data(), staging_desc.size);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_desc.size);
		copy_info.setSrcOffset(staging_node_info_buffer.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		loader->m_cmd.copyBuffer(staging_node_info_buffer.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

	}

	if (!m_resource->mBone.empty())
	{
		// BoneInfo
		{
			btr::BufferMemory::Descriptor staging_desc;
			staging_desc.size = m_resource->mBone.size() * sizeof(BoneInfo);
			staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			btr::AllocatedMemory staging_bone_info = loader->m_staging_memory.allocateMemory(staging_desc);
			auto* bo = static_cast<BoneInfo*>(staging_bone_info.getMappedPtr());
			for (size_t i = 0; i < m_resource->mBone.size(); i++) {
				bo[i].mBoneOffset = m_resource->mBone[i].mOffset;
				bo[i].mNodeIndex = m_resource->mBone[i].mNodeIndex;
			}
			auto& buffer = m_resource_instancing->getBuffer(BONE_INFO);
			buffer = loader->m_storage_memory.allocateMemory(m_resource->mBone.size() * sizeof(BoneInfo));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_bone_info.getBufferInfo().range);
			copy_info.setSrcOffset(staging_bone_info.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			loader->m_cmd.copyBuffer(staging_bone_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}

		// BoneTransform
		{
			auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM);
			buffer = loader->m_storage_memory.allocateMemory(m_resource->mBone.size() * instanceNum * sizeof(BoneTransformBuffer));
		}
	}

	{
		// staging buffer
		btr::BufferMemory::Descriptor desc;
		desc.size = instanceNum * sizeof(glm::mat4) * sGlobal::FRAME_MAX;
		m_resource_instancing->m_world_staging = loader->m_staging_memory.allocateMemory(desc);
		desc.size = sizeof(ModelInstancingInfo) * sGlobal::FRAME_MAX;
		m_resource_instancing->m_instancing_info = loader->m_staging_memory.allocateMemory(desc);
	}
	// PlayingAnimation
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = instanceNum * sizeof(PlayingAnimation);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_playing_animation = loader->m_staging_memory.allocateMemory(staging_desc);

		auto* pa = static_cast<PlayingAnimation*>(staging_playing_animation.getMappedPtr());
		for (int i = 0; i < instanceNum; i++)
		{
			pa[i].playingAnimationNo = 0;
			pa[i].isLoop = true;
			pa[i].time = (float)(std::rand() % 200);
			pa[i].currentMotionInfoIndex = 0;
		}

		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::PLAYING_ANIMATION);
		buffer = loader->m_storage_memory.allocateMemory(instanceNum * sizeof(PlayingAnimation));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_playing_animation.getBufferInfo().range);
		copy_info.setSrcOffset(staging_playing_animation.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		loader->m_cmd.copyBuffer(staging_playing_animation.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
	}

	// ModelInfo
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = sizeof(cModel::ModelInfo);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_model_info = loader->m_staging_memory.allocateMemory(staging_desc);

		auto& mi = *static_cast<cModel::ModelInfo*>(staging_model_info.getMappedPtr());
		mi.mNodeNum = (s32)m_resource->mNodeRoot.mNodeList.size();
		mi.mBoneNum = (s32)m_resource->mBone.size();
		mi.mMeshNum = (s32)m_resource->m_mesh.size();
		mi.m_node_depth_max = 0;
		for (auto& n : nodeInfo) {
			mi.m_node_depth_max = std::max(n.m_depth, mi.m_node_depth_max);
		}
		// todo
		glm::vec3 max(-10e10f);
		glm::vec3 min(10e10f);
		mi.mAabb = glm::vec4((max - min).xyz, glm::length((max - min)));

		mi.mInvGlobalMatrix = glm::inverse(m_resource->mNodeRoot.getRootNode()->mTransformation);

		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INFO);
		buffer = loader->m_uniform_memory.allocateMemory(sizeof(cModel::ModelInfo));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_model_info.getBufferInfo().range);
		copy_info.setSrcOffset(staging_model_info.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		loader->m_cmd.copyBuffer(staging_model_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

		m_resource->m_model_info = mi;
	}

	//ModelInstancingInfo
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO);
		buffer = loader->m_storage_memory.allocateMemory(sizeof(ModelInstancingInfo));
	}
	//BoneMap
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP);
		buffer = loader->m_storage_memory.allocateMemory(instanceNum * sizeof(s32));
	}

	//	NodeLocalTransformBuffer
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_LOCAL_TRANSFORM);
		buffer = loader->m_storage_memory.allocateMemory(m_resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(NodeLocalTransformBuffer));
	}


	//	NodeGlobalTransformBuffer
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_GLOBAL_TRANSFORM);
		buffer = loader->m_storage_memory.allocateMemory(m_resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(NodeGlobalTransformBuffer));
	}
	// world
	{
		auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::WORLD);
		buffer = loader->m_storage_memory.allocateMemory(instanceNum * sizeof(glm::mat4));
	}

	{
		auto& buffer = m_resource_instancing->m_compute_indirect_buffer;
		buffer = loader->m_vertex_memory.allocateMemory(sizeof(glm::ivec3) * 6);

		auto staging_compute = loader->m_staging_memory.allocateMemory(sizeof(glm::ivec3) * 6);
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
		loader->m_cmd.copyBuffer(staging_compute.getBufferInfo().buffer, m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().buffer, copy_info);

		vk::BufferMemoryBarrier dispatch_indirect_barrier;
		dispatch_indirect_barrier.setBuffer(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().buffer);
		dispatch_indirect_barrier.setOffset(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().offset);
		dispatch_indirect_barrier.setSize(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().range);
		dispatch_indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
		loader->m_cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			{}, { dispatch_indirect_barrier }, {});
	}

	{
		auto& anim = m_resource->getAnimation();
		m_resource_instancing->m_motion_texture = createMotion(loader, m_resource->getAnimation(), m_resource->mNodeRoot);

		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = sizeof(ModelRender::AnimationInfo) * anim.m_motion.size();
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = loader->m_staging_memory.allocateMemory(staging_desc);
		auto* staging_ptr = staging.getMappedPtr<ModelRender::AnimationInfo>();
		for (size_t i = 0; i < anim.m_motion.size(); i++)
		{
			ModelRender::AnimationInfo& animation = staging_ptr[i];
			animation.duration_ = (float)anim.m_motion[i].m_duration;
			animation.ticksPerSecond_ = (float)anim.m_motion[i].m_ticks_per_second;
		}
		// AnimeInfo
		{
			auto& buffer = m_resource_instancing->getBuffer(ANIMATION_INFO);
			btr::BufferMemory::Descriptor arg;
			arg.size = sizeof(ModelRender::AnimationInfo) * anim.m_motion.size();
			buffer = loader->m_uniform_memory.allocateMemory(arg);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier barrier;
			barrier.setBuffer(buffer.getBufferInfo().buffer);
			barrier.setOffset(buffer.getBufferInfo().offset);
			barrier.setSize(buffer.getBufferInfo().range);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			loader->m_cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(),
				{}, { barrier }, {});
		}
	}

}

void ModelRender::setup(cModelRenderer&  renderer)
{
	// setup draw
	{
		cDevice graphics_device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_GRAPHICS];
		auto& pipeline = renderer.getComputePipeline();

		{
			// meshごとの更新
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = pipeline.m_descriptor_pool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &pipeline.m_descriptor_set_layout[cModelComputePipeline::DESCRIPTOR_PER_MESH];
			m_draw_descriptor_set_per_mesh = graphics_device->allocateDescriptorSets(allocInfo);
			for (size_t i = 0; i < m_draw_descriptor_set_per_mesh.size(); i++)
			{
				auto& material = m_resource->m_material[m_resource->m_mesh[i].m_material_index];

				std::vector<vk::DescriptorImageInfo> color_image_info = {
					vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
				};
				std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(color_image_info.size())
					.setPImageInfo(color_image_info.data())
					.setDstBinding(0)
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
		m_descriptor_set = compute_device->allocateDescriptorSets(descriptor_set_alloc_info);

		vk::WriteDescriptorSet desc;
		// ModelInfo
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INFO).getBufferInfo(),
			};

			desc = vk::WriteDescriptorSet();
			desc.setDescriptorType(vk::DescriptorType::eUniformBuffer);
			desc.setDescriptorCount(uniforms.size());
			desc.setPBufferInfo(uniforms.data());
			desc.setDstBinding(0);
			desc.setDstSet(m_descriptor_set[cModelComputePipeline::DESCRIPTOR_MODEL]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				m_resource_instancing->getBuffer(MODEL_INSTANCING_INFO).getBufferInfo(),
				m_resource_instancing->getBuffer(BONE_TRANSFORM).getBufferInfo(),
				m_resource_instancing->getBuffer(MATERIAL).getBufferInfo(),
			};
			desc = vk::WriteDescriptorSet();
			desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			desc.setDescriptorCount(storages.size());
			desc.setPBufferInfo(storages.data());
			desc.setDstBinding(1);
			desc.setDstSet(m_descriptor_set[cModelComputePipeline::DESCRIPTOR_MODEL]);
			compute_device->updateDescriptorSets(desc, {});
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
			desc = vk::WriteDescriptorSet();
			desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			desc.setDescriptorCount(storages.size());
			desc.setPBufferInfo(storages.data());
			desc.setDstBinding(0);
			desc.setDstSet(m_descriptor_set[cModelComputePipeline::DESCRIPTOR_ANIMATION]);
			compute_device->updateDescriptorSets(desc, {});

			std::vector<vk::DescriptorImageInfo> images =
			{
				vk::DescriptorImageInfo()
				.setImageView(m_resource_instancing->m_motion_texture[0].getImageView())
				.setSampler(m_resource_instancing->m_motion_texture[0].m_resource->m_sampler)
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(images.size())
				.setPImageInfo(images.data())
				.setDstBinding(32)
				.setDstSet(m_descriptor_set[cModelComputePipeline::DESCRIPTOR_ANIMATION]);
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
			auto& staging = m_resource_instancing->m_world_staging;
			auto* world_ptr = static_cast<glm::mat4*>(staging.getMappedPtr()) + m_resource_instancing->m_instance_max_num*sGlobal::Order().getCurrentFrame();
			uint32_t model_count = 0;
			for (auto& m : m_model)
			{
				world_ptr[model_count] = m->getInstance()->m_world;
				model_count++;
			}
			auto& buffer = m_resource_instancing->getBuffer(ModelStorageBuffer::WORLD);

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
			to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
			to_copy_barrier.setSize(buffer.getBufferInfo().range);
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			vk::BufferCopy copy_info;
			copy_info.setSize(m_model.size() * sizeof(glm::mat4));
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
			model_info_ptr->mInstanceAliveNum = m_model.size();
			model_info_ptr->mInstanceMaxNum = m_resource_instancing->m_instance_max_num;
			model_info_ptr->mInstanceNum = 1000;
//			staging.subupdate<ModelInstancingInfo>(info);

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
				.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO).getBufferInfo().buffer)
				.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO).getBufferInfo().offset)
				.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO).getBufferInfo().range),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP).getBufferInfo().buffer)
				.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP).getBufferInfo().offset)
				.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP).getBufferInfo().range),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
				.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().buffer)
				.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().offset)
				.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().range),
				m_resource->m_mesh_resource.m_indirect_buffer_ex.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(), {}, barrier, {});

		}
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline[i]);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[cModelComputePipeline::PIPELINE_LAYOUT_COMPUTE], 0, m_descriptor_set[cModelComputePipeline::DESCRIPTOR_MODEL], {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.m_pipeline_layout[cModelComputePipeline::PIPELINE_LAYOUT_COMPUTE], 1, m_descriptor_set[cModelComputePipeline::DESCRIPTOR_ANIMATION], {});
		cmd.dispatchIndirect(m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().buffer, m_resource_instancing->m_compute_indirect_buffer.getBufferInfo().offset + i* 12);


		if (i == 3)
		{ 
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo().buffer)
				.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo().offset)
				.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_LOCAL_TRANSFORM).getBufferInfo().range),
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
				.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO).getBufferInfo().buffer)
				.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO).getBufferInfo().offset)
				.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::MODEL_INSTANCING_INFO).getBufferInfo().range),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP).getBufferInfo().buffer)
				.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP).getBufferInfo().offset)
				.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_MAP).getBufferInfo().range),
				vk::BufferMemoryBarrier()
				.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
				.setDstAccessMask(vk::AccessFlagBits::eShaderRead)
				.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo().buffer)
				.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo().offset)
				.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::NODE_GLOBAL_TRANSFORM).getBufferInfo().range),
				m_resource->m_mesh_resource.m_indirect_buffer_ex.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
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
		.setBuffer(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().buffer)
		.setOffset(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().offset)
		.setSize(m_resource_instancing->getBuffer(ModelStorageBuffer::BONE_TRANSFORM).getBufferInfo().range),
		m_resource->m_mesh_resource.m_indirect_buffer_ex.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead),
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlags(), {}, to_draw_barrier, {});

}

void ModelRender::draw(cModelRenderer& renderer, vk::CommandBuffer& cmd)
{
	auto& pipeline = renderer.getComputePipeline();
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.m_graphics_pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelComputePipeline::PIPELINE_LAYOUT_RENDER], 2, pipeline.m_descriptor_set_scene, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelComputePipeline::PIPELINE_LAYOUT_RENDER], 0, m_descriptor_set[cModelComputePipeline::DESCRIPTOR_MODEL], {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelComputePipeline::PIPELINE_LAYOUT_RENDER], 3, pipeline.m_descriptor_set_light, {});
	for (auto m : m_resource->m_mesh)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.m_pipeline_layout[cModelComputePipeline::PIPELINE_LAYOUT_RENDER], 1, m_draw_descriptor_set_per_mesh[m.m_material_index], {});
		cmd.pushConstants<uint32_t>(pipeline.m_pipeline_layout[cModelComputePipeline::PIPELINE_LAYOUT_RENDER], vk::ShaderStageFlagBits::eFragment, 0, m.m_material_index);
 		cmd.bindVertexBuffers(0, { m_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { m_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
 		cmd.bindIndexBuffer(m_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, m_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, m_resource->m_mesh_resource.mIndexType);
 		cmd.drawIndexedIndirect(m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, m_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, m_resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));
	}
}
