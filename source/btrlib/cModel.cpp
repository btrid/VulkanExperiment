#include <btrlib/Define.h>
#include <chrono>
#include <filesystem>
#include <algorithm>

#include <btrlib/cModel.h>
#include <btrlib/sGlobal.h>
#include <btrlib/sDebug.h>
#include <btrlib/cStopWatch.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

namespace {
	glm::mat4 AI_TO(aiMatrix4x4& from)
	{
		glm::mat4 to;
		to[0].x = from.a1;
		to[0].y = from.a2;
		to[0].z = from.a3;
		to[0].w = from.a4;

		to[1].x = from.b1;
		to[1].y = from.b2;
		to[1].z = from.b3;
		to[1].w = from.b4;

		to[2].x = from.c1;
		to[2].y = from.c2;
		to[2].z = from.c3;
		to[2].w = from.c4;

		to[3].x = from.d1;
		to[3].y = from.d2;
		to[3].z = from.d3;
		to[3].w = from.d4;

		return glm::transpose(to);
	}

	int countAiNode(aiNode* ainode)
	{
		int count = 1;
		for (size_t i = 0; i < ainode->mNumChildren; i++) {
			count += countAiNode(ainode->mChildren[i]);
		}
		return count;
	}

}

/**
 *	モーションのデータを一枚の1DArrayに格納
 */
MotionTexture create(ModelLoader* loader, const aiAnimation* anim, const RootNode& root)
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
	staging_desc.size = image_info.arrayLayers * anim->mNumChannels * SIZE * sizeof(glm::u16vec4);
	staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_buffer = loader->m_staging_memory.allocateMemory(staging_desc);
	auto* data = staging_buffer.getMappedPtr<glm::u16vec4>();

	for (size_t i = 0; i < anim->mNumChannels; i++)
	{
		aiNodeAnim* aiAnim = anim->mChannels[i];
		auto no = root.getNodeIndexByName(aiAnim->mNodeName.C_Str());
		auto* pos = reinterpret_cast<uint64_t*>(data + no * 3 * SIZE);
		auto* quat = reinterpret_cast<uint64_t*>(pos + SIZE);
		auto* scale = reinterpret_cast<uint64_t*>(quat + SIZE);

		float time_max = (float)anim->mDuration;// / anim->mTicksPerSecond;
		float time_per = time_max / SIZE;
		float time = 0.;
		int count = 0;
		for (size_t n = 0; n < aiAnim->mNumScalingKeys - 1;)
		{
			auto& current = aiAnim->mScalingKeys[n];
 			auto& next = aiAnim->mScalingKeys[n + 1];

			if (time >= next.mTime)
			{
				n++;
				continue;
			}
			auto current_value = glm::vec4(current.mValue.x, current.mValue.y, current.mValue.z, 0.f);
			auto next_value = glm::vec4(next.mValue.x, next.mValue.y, next.mValue.z, 0.f);
			auto rate = 1.f - (float)(next.mTime - time) / (float)(next.mTime - current.mTime);
			auto value = glm::lerp(current_value, next_value, rate);
			scale[count] = glm::packHalf4x16(value);
			count++;
			time += time_per;
		}
		assert(count == SIZE);
		time = 0.;
		count = 0;
		for (size_t n = 0; n < aiAnim->mNumPositionKeys - 1;)
		{
			auto& current = aiAnim->mPositionKeys[n];
			auto& next = aiAnim->mPositionKeys[n + 1];

			if (time >= next.mTime)
			{
				n++;
				continue;
			}
			auto current_value = glm::vec3(current.mValue.x, current.mValue.y, current.mValue.z);
			auto next_value = glm::vec3(next.mValue.x, next.mValue.y, next.mValue.z);
			auto rate = 1.f - (float)(next.mTime - time) / (float)(next.mTime - current.mTime);
			auto value = glm::lerp<float>(current_value, next_value, rate);
			pos[count] = glm::packHalf4x16(glm::vec4(value, 0.f));
			count++;
			time += time_per;
		}
		assert(count == SIZE);
		time = 0.;
		count = 0;
		for (size_t n = 0; n < aiAnim->mNumRotationKeys - 1;)
		{
			auto& current = aiAnim->mRotationKeys[n];
			auto& next = aiAnim->mRotationKeys[n + 1];

			if (time >= next.mTime)
			{
				n++;
				continue;
			}
			auto current_value = glm::quat(current.mValue.w, current.mValue.x, current.mValue.y, current.mValue.z);
			auto next_value = glm::quat(next.mValue.w, next.mValue.x, next.mValue.y, next.mValue.z);
			auto rate = 1.f - (float)(next.mTime - time) / (float)(next.mTime - current.mTime);
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


ResourceManager<ResourceTexture::Resource> ResourceTexture::s_manager;
ResourceManager<cModel::Resource> cModel::s_manager;
void ResourceTexture::load(ModelLoader* loader, cThreadPool& thread_pool, const std::string& filename)
{
	if (s_manager.manage(m_private, filename)) {
		return;
	}
	m_private->m_device = loader->m_device;

	auto texture_data = rTexture::LoadTexture(filename);
	vk::ImageCreateInfo image_info;
	image_info.imageType = vk::ImageType::e2D;
	image_info.format = vk::Format::eR32G32B32A32Sfloat;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = vk::SampleCountFlagBits::e1;
	image_info.tiling = vk::ImageTiling::eLinear;
	image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	image_info.sharingMode = vk::SharingMode::eExclusive;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.extent = { texture_data.m_size.x, texture_data.m_size.y, 1 };
	image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
	vk::Image image = loader->m_device->createImage(image_info);

	vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(image);
	vk::MemoryAllocateInfo memory_alloc_info;
	memory_alloc_info.allocationSize = memory_request.size;
	memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::DeviceMemory image_memory = loader->m_device->allocateMemory(memory_alloc_info);
	loader->m_device->bindImageMemory(image, image_memory, 0);

	btr::BufferMemory::Descriptor staging_desc;
	staging_desc.size = texture_data.getBufferSize();
	staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_buffer = loader->m_staging_memory.allocateMemory(staging_desc);	
	memcpy(staging_buffer.getMappedPtr(), texture_data.m_data.data(), texture_data.getBufferSize());

	vk::ImageSubresourceRange subresourceRange;
	subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.layerCount = 1;
	subresourceRange.levelCount = 1;

	{
		// staging_bufferからimageへコピー

		vk::BufferImageCopy copy;
		copy.bufferOffset = staging_buffer.getBufferInfo().offset;
		copy.imageExtent = { texture_data.m_size.x, texture_data.m_size.y, texture_data.m_size.z };
		copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
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

	}

	vk::ImageViewCreateInfo view_info;
	view_info.viewType = vk::ImageViewType::e2D;
	view_info.components.r = vk::ComponentSwizzle::eR;
	view_info.components.g = vk::ComponentSwizzle::eG;
	view_info.components.b = vk::ComponentSwizzle::eB;
	view_info.components.a = vk::ComponentSwizzle::eA;
	view_info.flags = vk::ImageViewCreateFlags();
	view_info.format = vk::Format::eR32G32B32A32Sfloat;
	view_info.image = image;
	view_info.subresourceRange = subresourceRange;

	vk::SamplerCreateInfo sampler_info;
	sampler_info.magFilter = vk::Filter::eNearest;
	sampler_info.minFilter = vk::Filter::eNearest;
	sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.compareOp = vk::CompareOp::eNever;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.f;
	sampler_info.maxAnisotropy = 1.0;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

	m_private->m_image = image;
	m_private->m_memory = image_memory;
	m_private->m_image_view = loader->m_device->createImageView(view_info);
	m_private->m_sampler = loader->m_device->createSampler(sampler_info);
}


std::vector<cModel::Material> loadMaterial(const aiScene* scene, const std::string& filename, ModelLoader* loader)
{
	std::string path = std::tr2::sys::path(filename).remove_filename().string();
	std::vector<cModel::Material> material(scene->mNumMaterials);
	for (size_t i = 0; i < scene->mNumMaterials; i++)
	{
		auto* aiMat = scene->mMaterials[i];
		auto& mat = material[i];
		aiColor4D color;
#define _copy(ai, to) to.a = ai.a; to.r = ai.r; to.g = ai.g; to.b = ai.b;
		aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
		_copy(color, mat.mDiffuse);
		aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color);
		_copy(color, mat.mAmbient);
		aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color);
		_copy(color, mat.mSpecular);
		aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color);
		_copy(color, mat.mEmissive);
		aiMat->Get(AI_MATKEY_SHININESS, mat.mShininess);
#undef _copy
		aiString str;
		aiTextureMapMode mapmode[3];
		aiTextureMapping mapping;
		unsigned uvIndex;
		if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
			mat.mDiffuseTex.load(loader, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mAmbientTex.load(loader, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mSpecularTex.load(loader, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mNormalTex.load(loader, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mHeightTex.load(loader, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}
	}
	return material;
}

void _loadNodeRecurcive(aiNode* ainode, RootNode& root, int parent)
{
	auto nodeIndex = (s32)root.mNodeList.size();
	Node node;
	node.mName = ainode->mName.C_Str();
	node.mTransformation = AI_TO(ainode->mTransformation);
	node.mParent = parent;
	if (parent >= 0) {
		root.mNodeList[parent].mChildren.push_back(nodeIndex);
	}
	root.mNodeList.push_back(node);

	for (s32 i = 0; i < (int)ainode->mNumChildren; i++) {
		_loadNodeRecurcive(ainode->mChildren[i], root, nodeIndex);
	}
}
RootNode loadNode(const aiScene* scene)
{
	RootNode root;
	root.mNodeList.clear();
	root.mNodeList.reserve(countAiNode(scene->mRootNode));
	_loadNodeRecurcive(scene->mRootNode, root, -1);
	return root;
}

void loadNodeBufferRecurcive(aiNode* ainode, std::vector<cModel::NodeInfo>& nodeBuffer, int parentIndex)
{
	nodeBuffer.emplace_back();
	auto& node = nodeBuffer.back();
	node.mNodeNo = (s32)nodeBuffer.size() - 1;
	node.mParent = parentIndex;
	node.m_depth = nodeBuffer[parentIndex].m_depth + 1;
	for (size_t i = 0; i < ainode->mNumChildren; i++) {
		loadNodeBufferRecurcive(ainode->mChildren[i], nodeBuffer, node.mNodeNo);
	}
}

std::vector<cModel::NodeInfo> loadNodeInfo(aiNode* ainode)
{
	std::vector<cModel::NodeInfo> nodeBuffer;
	nodeBuffer.reserve(countAiNode(ainode));
	nodeBuffer.emplace_back();
	auto& node = nodeBuffer.back();
	node.mNodeNo = (int32_t)nodeBuffer.size() - 1;
	node.mParent = -1;
	node.m_depth = 0;
	for (size_t i = 0; i < ainode->mNumChildren; i++) {
		loadNodeBufferRecurcive(ainode->mChildren[i], nodeBuffer, node.mNodeNo);
	}

	return nodeBuffer;
}

void loadMotion(cAnimation& anim_buffer, const aiScene* scene, const RootNode& root, ModelLoader* loader)
{
	if (!scene->HasAnimations()) {
		return;
	}

	btr::BufferMemory::Descriptor staging_desc;
	staging_desc.size = sizeof(cModel::AnimationInfo) * scene->mNumAnimations;
	staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging = loader->m_staging_memory.allocateMemory(staging_desc);
	auto* staging_ptr = staging.getMappedPtr<cModel::AnimationInfo>();
	for (size_t i = 0; i < scene->mNumAnimations; i++)
	{
		aiAnimation* anim = scene->mAnimations[i]; 
		{
			anim_buffer.m_motion_texture = create(loader, anim, root);
		}

		cModel::AnimationInfo& animation = staging_ptr[i];
		animation.maxTime_ = (float)anim->mDuration;
		animation.ticksPerSecond_ = (float)anim->mTicksPerSecond;


	}

	// AnimeInfo
	{
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::ANIMATION_INFO];
		btr::BufferMemory::Descriptor arg;
		arg.size = sizeof(cModel::AnimationInfo) * scene->mNumAnimations;
//		arg.attribute = btr::BufferMemory::AttributeFlagBits::MEMORY_CONSTANT;
		buffer = loader->m_storage_uniform_memory.allocateMemory(arg);

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
			vk::PipelineStageFlagBits::eVertexInput,
			vk::DependencyFlags(),
			{}, { barrier }, {});
	}

}


cModel::cModel()
{
}
cModel::~cModel()
{}


void cModel::load(const std::string& filename)
{

	m_instance = std::make_unique<Instance>();
	if (s_manager.manage(m_resource, filename)) {
		return;
	}
	auto s = std::chrono::system_clock::now();

	int OREORE_PRESET = 0
		| aiProcess_JoinIdenticalVertices
		| aiProcess_ImproveCacheLocality
		| aiProcess_LimitBoneWeights
		| aiProcess_RemoveRedundantMaterials
		| aiProcess_SplitLargeMeshes
		| aiProcess_SortByPType
//		| aiProcess_OptimizeMeshes
		| aiProcess_Triangulate
//		| aiProcess_MakeLeftHanded
		;
	cStopWatch timer;
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filename, OREORE_PRESET);
	if (!scene) {
		sDebug::Order().print(sDebug::FLAG_ERROR | sDebug::ACTION_ASSERTION,"can't file load in cModel::load : %s : %s\n", filename.c_str(), importer.GetErrorString());
		return;
	}
	sDebug::Order().print(sDebug::FLAG_LOG | sDebug::SOURCE_MODEL, "[Load Model %6.2fs] %s \n", timer.getElapsedTimeAsSeconds(), filename.c_str());


	auto device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_GRAPHICS];
	m_loader = std::make_unique<ModelLoader>();
	m_loader->m_device = device;
	m_loader->m_vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, 128 * 65536);
	m_loader->m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, 1024 * 1024 * 20);
	m_loader->m_storage_uniform_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, 8192);
	m_loader->m_staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached, 1024 * 1024 * 20);

	m_loader->m_vertex_memory.setName("Model Vertex Memory");
	m_loader->m_storage_memory.setName("Model Storage Memory");
	m_loader->m_storage_uniform_memory.setName("Model Uniform Memory");
	m_loader->m_staging_memory.setName("Model Staging Memory");
	int instanceNum = 1000;
	{
		// staging buffer
		m_resource->m_world_staging_buffer = m_loader->m_staging_memory.allocateMemory(instanceNum * sizeof(glm::mat4) * sGlobal::FRAME_MAX);
		m_resource->m_model_info_staging_buffer = m_loader->m_staging_memory.allocateMemory(sizeof(ModelInfo) * sGlobal::FRAME_MAX);

	}

	auto cmd_pool = sGlobal::Order().getCmdPoolTempolary(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));
	vk::CommandBufferAllocateInfo cmd_info;
	cmd_info.setCommandPool(cmd_pool);
	cmd_info.setLevel(vk::CommandBufferLevel::ePrimary);
	cmd_info.setCommandBufferCount(1);
	auto cmd = device->allocateCommandBuffers(cmd_info)[0];
	vk::CommandBufferBeginInfo begin_info;
	begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmd.begin(begin_info);

	m_loader->m_cmd = cmd;
	// 初期化
	m_resource->mMeshNum = scene->mNumMeshes;
	m_resource->m_material = loadMaterial(scene, filename, m_loader.get());

	m_resource->mNodeRoot = loadNode(scene);
	loadMotion(m_resource->m_animation_buffer, scene, m_resource->mNodeRoot, m_loader.get());

	std::vector<NodeInfo> nodeInfo = loadNodeInfo(scene->mRootNode);

	std::vector<Bone>& boneList = m_resource->mBone;

	m_resource->m_material_index.resize(scene->mNumMeshes);
	unsigned numIndex = 0;
	unsigned numVertex = 0;
	std::vector<int> vertexSize(scene->mNumMeshes);
	std::vector<int> indexSize(scene->mNumMeshes);
	std::vector<int>& materialIndex = m_resource->m_material_index;
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		numVertex += scene->mMeshes[i]->mNumVertices;
		numIndex += scene->mMeshes[i]->mNumFaces * 3;
		vertexSize[i] = scene->mMeshes[i]->mNumVertices;
		indexSize[i] = scene->mMeshes[i]->mNumFaces * 3;
	}

	btr::BufferMemory::Descriptor staging_vertex_desc;
	staging_vertex_desc.size = sizeof(Vertex) * numVertex;
	staging_vertex_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_vertex = m_loader->m_staging_memory.allocateMemory(staging_vertex_desc);

	auto index_type = numVertex < std::numeric_limits<uint16_t>::max() ? vk::IndexType::eUint16 : vk::IndexType::eUint32;

	btr::BufferMemory::Descriptor staging_index_desc;
	staging_index_desc.size = (index_type == vk::IndexType::eUint16 ? sizeof(uint16_t) : sizeof(uint32_t)) * numIndex;
	staging_index_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_index = m_loader->m_staging_memory.allocateMemory(staging_index_desc);
	auto index_stride = (index_type == vk::IndexType::eUint16 ? sizeof(uint16_t) : sizeof(uint32_t));
	auto* index = static_cast<char*>(staging_index.getMappedPtr());
	Vertex* vertex = static_cast<Vertex*>(staging_vertex.getMappedPtr());
	memset(vertex, -1, staging_vertex.getBufferInfo().range);
	uint32_t v_count = 0;
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[i];
		materialIndex[i] = mesh->mMaterialIndex;

		// ELEMENT_ARRAY_BUFFER
		// 三角メッシュとして読み込む
		for (u32 n = 0; n < mesh->mNumFaces; n++) {
			(*(uint32_t*)index) = mesh->mFaces[n].mIndices[0] + v_count;
			index += index_stride;
			(*(uint32_t*)index) = mesh->mFaces[n].mIndices[1] + v_count;
			index += index_stride;
			(*(uint32_t*)index) = mesh->mFaces[n].mIndices[2] + v_count;
			index += index_stride;
		}

		// ARRAY_BUFFER
		Vertex* _vertex = vertex + v_count;
		v_count += mesh->mNumVertices;
		for (size_t v = 0; v < mesh->mNumVertices; v++){
			_vertex[v].m_position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
		}
		if (mesh->HasNormals()) {
			for (size_t v = 0; v < mesh->mNumVertices; v++) {
				_vertex[v].m_normal = glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
			}
		}
		if (mesh->HasTextureCoords(0)) {
			for (size_t v = 0; v < mesh->mNumVertices; v++) {
				_vertex[v].m_texcoord0[0] = glm::packSnorm1x8(mesh->mTextureCoords[0][v].x);
				_vertex[v].m_texcoord0[1] = glm::packSnorm1x8(mesh->mTextureCoords[0][v].y);
				_vertex[v].m_texcoord0[2] = glm::packSnorm1x8(mesh->mTextureCoords[0][v].z);
				_vertex[v].m_texcoord0[3] = glm::packSnorm1x8(0.f);
			}
		}

		// SkinMesh
		if (mesh->HasBones())
		{
			for (size_t b = 0; b < mesh->mNumBones; b++)
			{
				// BoneがMeshからしか参照できないので全部展開する
				u8 index = 0xffui8;
				for (size_t k = 0; k < boneList.size(); k++) {
					if (boneList[k].mName.compare(mesh->mBones[b]->mName.C_Str()) == 0) {
						index = (int)k;
						break;
					}

				}
				if (index == 0xffui8) {
					// 新しいボーンの登録
					Bone bone;
					bone.mName = mesh->mBones[b]->mName.C_Str();
					bone.mOffset = AI_TO(mesh->mBones[b]->mOffsetMatrix);
					bone.mNodeIndex = m_resource->mNodeRoot.getNodeIndexByName(mesh->mBones[b]->mName.C_Str());
					boneList.emplace_back(bone);
					index = (int)boneList.size() - 1;
					assert(index < 0xffui8);
					nodeInfo[bone.mNodeIndex].mBoneIndex = index;
				}

				for (size_t i = 0; i < mesh->mBones[b]->mNumWeights; i++)
				{
					aiVertexWeight& weight = mesh->mBones[b]->mWeights[i];
					Vertex& v = _vertex[weight.mVertexId];
					for (glm::length_t o = 0; o < Vertex::BONE_NUM; o++) {
						if (v.m_bone_ID[o] == 0xffui8) {
							v.m_bone_ID[o] = index;
							v.m_weight[o] = glm::packUnorm1x8(weight.mWeight);
							break;
						}
					}
				}
			}
		}
	}
	importer.FreeScene();

	const cGPU& gpu = sThreadLocal::Order().m_gpu;
	auto familyIndex = device.getQueueFamilyIndex();

	{
		cMeshResource& mesh = m_resource->mMesh;

		{
			mesh.m_vertex_buffer_ex = m_loader->m_vertex_memory.allocateMemory(staging_vertex.getBufferInfo().range);
			mesh.m_index_buffer_ex = m_loader->m_vertex_memory.allocateMemory(staging_index.getBufferInfo().range);

			btr::BufferMemory::Descriptor indirect_desc;
			indirect_desc.size = sizeof(Mesh) * indexSize.size();
			mesh.m_indirect_buffer_ex = m_loader->m_vertex_memory.allocateMemory(indirect_desc);

			indirect_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_indirect = m_loader->m_staging_memory.allocateMemory(indirect_desc);
			auto* indirect = staging_indirect.getMappedPtr<Mesh>(0);
			int offset = 0;
			for (size_t i = 0; i < indexSize.size(); i++) {
				indirect[i].m_draw_cmd.indexCount = indexSize[i];
				indirect[i].m_draw_cmd.firstIndex = offset;
				indirect[i].m_draw_cmd.instanceCount = 1000;
				indirect[i].m_draw_cmd.vertexOffset = 0;
				indirect[i].m_draw_cmd.firstInstance = 0;
				offset += indexSize[i];
			}
			vk::BufferCopy copy_info;
			copy_info.setSize(staging_vertex.getBufferInfo().range);
			copy_info.setSrcOffset(staging_vertex.getBufferInfo().offset);
			copy_info.setDstOffset(mesh.m_vertex_buffer_ex.getBufferInfo().offset);
			cmd.copyBuffer(staging_vertex.getBufferInfo().buffer, mesh.m_vertex_buffer_ex.getBufferInfo().buffer, copy_info);

			copy_info.setSize(staging_index.getBufferInfo().range);
			copy_info.setSrcOffset(staging_index.getBufferInfo().offset);
			copy_info.setDstOffset(mesh.m_index_buffer_ex.getBufferInfo().offset);
			cmd.copyBuffer(staging_index.getBufferInfo().buffer, mesh.m_index_buffer_ex.getBufferInfo().buffer, copy_info);

			copy_info.setSize(staging_indirect.getBufferInfo().range);
			copy_info.setSrcOffset(staging_indirect.getBufferInfo().offset);
			copy_info.setDstOffset(mesh.m_indirect_buffer_ex.getBufferInfo().offset);
			cmd.copyBuffer(staging_indirect.getBufferInfo().buffer, mesh.m_indirect_buffer_ex.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier vertex_barrier;
			vertex_barrier.setBuffer(mesh.m_vertex_buffer_ex.getBufferInfo().buffer);
			vertex_barrier.setOffset(mesh.m_vertex_buffer_ex.getBufferInfo().offset);
			vertex_barrier.setSize(mesh.m_vertex_buffer_ex.getBufferInfo().range);
			vertex_barrier.setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead);
			vk::BufferMemoryBarrier index_barrier;
			index_barrier.setBuffer(mesh.m_index_buffer_ex.getBufferInfo().buffer);
			index_barrier.setOffset(mesh.m_index_buffer_ex.getBufferInfo().offset);
			index_barrier.setSize(mesh.m_index_buffer_ex.getBufferInfo().range);
			index_barrier.setDstAccessMask(vk::AccessFlagBits::eIndexRead);
			vk::BufferMemoryBarrier indirect_barrier;
			indirect_barrier.setBuffer(mesh.m_indirect_buffer_ex.getBufferInfo().buffer);
			indirect_barrier.setOffset(mesh.m_indirect_buffer_ex.getBufferInfo().offset);
			indirect_barrier.setSize(mesh.m_indirect_buffer_ex.getBufferInfo().range);
			indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eVertexInput,
				vk::DependencyFlags(),
				{}, { vertex_barrier, index_barrier}, {});
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::DependencyFlags(),
				{}, { indirect_barrier }, {});

			mesh.mIndexType = index_type;
			mesh.mIndirectCount = (int32_t)indexSize.size();
		}
	}

	// material
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = m_resource->m_material.size() * sizeof(MaterialBuffer);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_material = m_loader->m_staging_memory.allocateMemory(staging_desc);
		auto* mb = static_cast<MaterialBuffer*>(staging_material.getMappedPtr());
		for (size_t i = 0; i < m_resource->m_material.size(); i++)
		{
			mb[i].mAmbient = m_resource->m_material[i].mAmbient;
			mb[i].mDiffuse = m_resource->m_material[i].mDiffuse;
			mb[i].mEmissive = m_resource->m_material[i].mEmissive;
			mb[i].mShininess = m_resource->m_material[i].mShininess;
			mb[i].mSpecular = m_resource->m_material[i].mSpecular;
		}

		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::MATERIAL);
		buffer = m_loader->m_storage_memory.allocateMemory(m_resource->m_material.size() * sizeof(MaterialBuffer));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_material.getBufferInfo().range);
		copy_info.setSrcOffset(staging_material.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd.copyBuffer(staging_material.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

	}

	// node info
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = vector_sizeof(nodeInfo);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_node_info_buffer = m_loader->m_staging_memory.allocateMemory(staging_desc);
		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::NODE_INFO);
		buffer = m_loader->m_storage_memory.allocateMemory(staging_desc.size);

		memcpy_s(staging_node_info_buffer.getMappedPtr(), staging_desc.size, nodeInfo.data(), staging_desc.size);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_desc.size);
		copy_info.setSrcOffset(staging_node_info_buffer.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd.copyBuffer(staging_node_info_buffer.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

	}

	if (!m_resource->mBone.empty())
	{
		// BoneInfo
		{
			btr::BufferMemory::Descriptor staging_desc;
			staging_desc.size = m_resource->mBone.size() * sizeof(BoneInfo);
			staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			btr::AllocatedMemory staging_bone_info = m_loader->m_staging_memory.allocateMemory(staging_desc);
			auto* bo = static_cast<BoneInfo*>(staging_bone_info.getMappedPtr());
			for (size_t i = 0; i < m_resource->mBone.size(); i++) {
				bo[i].mBoneOffset = m_resource->mBone[i].mOffset;
				bo[i].mNodeIndex = m_resource->mBone[i].mNodeIndex;
			}
			auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::BONE_INFO);
			buffer = m_loader->m_storage_memory.allocateMemory(m_resource->mBone.size() * sizeof(BoneInfo));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_bone_info.getBufferInfo().range);
			copy_info.setSrcOffset(staging_bone_info.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging_bone_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}

		// BoneTransform
		{
			auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::BONE_TRANSFORM);
			buffer = m_loader->m_storage_memory.allocateMemory(m_resource->mBone.size() * instanceNum * sizeof(BoneTransformBuffer));
		}
	}

	// PlayingAnimation
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = instanceNum * sizeof(PlayingAnimation);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_playing_animation = m_loader->m_staging_memory.allocateMemory(staging_desc);

		auto* pa = static_cast<PlayingAnimation*>(staging_playing_animation.getMappedPtr());
		for (int i = 0; i < instanceNum; i++)
		{
			pa[i].playingAnimationNo = 0;
			pa[i].isLoop = true;
			pa[i].time = (float)(std::rand() % 200);
			pa[i].currentMotionInfoIndex = 0;
		}

		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::PLAYING_ANIMATION);
		buffer = m_loader->m_storage_memory.allocateMemory(instanceNum * sizeof(PlayingAnimation));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_playing_animation.getBufferInfo().range);
		copy_info.setSrcOffset(staging_playing_animation.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd.copyBuffer(staging_playing_animation.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
	}

	// ModelInfo
	{
		btr::BufferMemory::Descriptor staging_desc;
		staging_desc.size = sizeof(ModelInfo);
		staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_model_info = m_loader->m_staging_memory.allocateMemory(staging_desc);

		auto& mi = *static_cast<ModelInfo*>(staging_model_info.getMappedPtr());
		mi.mInstanceMaxNum = instanceNum;
		mi.mInstanceAliveNum = 1000;
		mi.mInstanceNum = 1000;
		mi.mNodeNum = (s32)m_resource->mNodeRoot.mNodeList.size();
		mi.mBoneNum = (s32)m_resource->mBone.size();
		mi.mMeshNum = m_resource->mMeshNum;
		mi.m_node_depth_max = 0;
		for (auto& n : nodeInfo) {
			mi.m_node_depth_max = std::max(n.m_depth, mi.m_node_depth_max);
		}
		glm::vec3 max(-10e10f);
		glm::vec3 min(10e10f);
		for (unsigned i = 0; i < numVertex; i++)
		{
			auto& v = vertex[i];
			max.x = glm::max(max.x, v.m_position.x);
			max.y = glm::max(max.y, v.m_position.y);
			max.z = glm::max(max.z, v.m_position.z);
			min.x = glm::min(min.x, v.m_position.x);
			min.y = glm::min(min.y, v.m_position.y);
			min.z = glm::min(min.z, v.m_position.z);
		}
		mi.mAabb = glm::vec4((max - min).xyz, glm::length((max - min)));
		mi.mInvGlobalMatrix = glm::inverse(m_resource->mNodeRoot.getRootNode()->mTransformation);

		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::MODEL_INFO);
		buffer = m_loader->m_storage_uniform_memory.allocateMemory(sizeof(ModelInfo));

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_model_info.getBufferInfo().range);
		copy_info.setSrcOffset(staging_model_info.getBufferInfo().offset);
		copy_info.setDstOffset(buffer.getBufferInfo().offset);
		cmd.copyBuffer(staging_model_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

		m_resource->m_model_info = mi;
	}

	//BoneMap
	{
		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::BONE_MAP);
		buffer = m_loader->m_storage_memory.allocateMemory(instanceNum * sizeof(s32));
	}

	//	NodeLocalTransformBuffer
	{
		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::NODE_LOCAL_TRANSFORM);
		buffer = m_loader->m_storage_memory.allocateMemory(m_resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(NodeLocalTransformBuffer));
	}


	//	NodeGlobalTransformBuffer
	{
		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::NODE_GLOBAL_TRANSFORM);
		buffer = m_loader->m_storage_memory.allocateMemory(m_resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(NodeGlobalTransformBuffer));
	}
	// world
	{
		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::WORLD);
		buffer = m_loader->m_storage_memory.allocateMemory(instanceNum * sizeof(glm::mat4));

	}

	m_resource->mVertexNum = std::move(vertexSize);
	m_resource->mIndexNum = std::move(indexSize);

	auto NodeNum = m_resource->mNodeRoot.mNodeList.size();
	auto BoneNum = m_resource->mBone.size();
	auto MeshNum = m_resource->mMeshNum;

	{
		// todo rendererに移動
		auto& buffer = m_resource->m_compute_indirect_buffer;
		buffer = m_loader->m_vertex_memory.allocateMemory(sizeof(glm::ivec3) * 6);

		auto staging_compute = m_loader->m_staging_memory.allocateMemory(sizeof(glm::ivec3) * 6);
		auto* group_ptr = static_cast<glm::ivec3*>(staging_compute.getMappedPtr());
		int32_t local_size_x = 1024;
		// shaderのlocal_size_xと合わせる
		std::vector<glm::ivec3> group =
		{
			glm::ivec3(1, 1, 1),
			glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mNodeNum + local_size_x-1)/local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mNodeNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum + local_size_x - 1)/local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mBoneNum + local_size_x - 1) / local_size_x, 1, 1),
		};
		memcpy_s(group_ptr, sizeof(glm::ivec3) * 6, group.data(), sizeof(glm::ivec3) * 6);

		vk::BufferCopy copy_info;
		copy_info.setSize(staging_compute.getBufferInfo().range);
		copy_info.setSrcOffset(staging_compute.getBufferInfo().offset);
		copy_info.setDstOffset(m_resource->m_compute_indirect_buffer.getBufferInfo().offset);
		cmd.copyBuffer(staging_compute.getBufferInfo().buffer, m_resource->m_compute_indirect_buffer.getBufferInfo().buffer, copy_info);

		vk::BufferMemoryBarrier dispatch_indirect_barrier;
		dispatch_indirect_barrier.setBuffer(m_resource->m_compute_indirect_buffer.getBufferInfo().buffer);
		dispatch_indirect_barrier.setOffset(m_resource->m_compute_indirect_buffer.getBufferInfo().offset);
		dispatch_indirect_barrier.setSize(m_resource->m_compute_indirect_buffer.getBufferInfo().range);
		dispatch_indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eComputeShader,
			vk::DependencyFlags(),
			{}, { dispatch_indirect_barrier}, {});
	}

	cmd.end();
	auto queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), device.getQueueNum(vk::QueueFlagBits::eGraphics)-1);
	vk::SubmitInfo submit_info;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;

	vk::FenceCreateInfo fence_info;
	vk::Fence fence = device->createFence(fence_info);

	queue.submit(submit_info, fence);
	queue.waitIdle();
	std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
	deleter->pool = cmd_pool;
	deleter->cmd.push_back(cmd);
	deleter->device = device.getHandle();
	deleter->fence = { fence };
	sGlobal::Order().destroyResource(std::move(deleter));

	auto e = std::chrono::system_clock::now();
	auto t = std::chrono::duration_cast<std::chrono::microseconds>(e - s);
	sDebug::Order().print(sDebug::FLAG_LOG | sDebug::SOURCE_MODEL, "[Load Complete %6.2fs] %s NodeNum = %d BoneNum = %d \n", t.count() / 1000.f / 1000.f, filename.c_str(), NodeNum, BoneNum);

}


std::string cModel::getFilename() const
{
	return m_resource ? m_resource->m_filename : "";
}
const cMeshResource* cModel::getMesh() const
{
	return m_resource ? &m_resource->mMesh : nullptr;
}
