#include <btrlib/Define.h>
#include <chrono>
#include <filesystem>
#include <algorithm>

#include <btrlib/cModel.h>
#include <btrlib/sGlobal.h>
#include <btrlib/ThreadPool.h>
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

		return std::move(glm::transpose(to));
	}
	glm::mat4 aiTo(aiMatrix4x4& from)
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


		return std::move(to);
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
ResourceManager<ResourceTexture::Resource> ResourceTexture::s_manager;
ResourceManager<cModel::Resource> cModel::s_manager;
void ResourceTexture::load(const cGPU& gpu, const cDevice& device, cThreadPool& thread_pool, const std::string& filename)
{
	if (s_manager.manage(m_private, filename)) {
		return;
	}
	m_gpu = gpu;
	m_private->m_device = device;

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
	vk::Image image = device->createImage(image_info);

	vk::MemoryRequirements memory_request = device->getImageMemoryRequirements(image);
	vk::MemoryAllocateInfo memory_alloc_info;
	memory_alloc_info.allocationSize = memory_request.size;
	memory_alloc_info.memoryTypeIndex = gpu.getMemoryTypeIndex(memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::DeviceMemory memory = device->allocateMemory(memory_alloc_info);
	device->bindImageMemory(image, memory, 0);

	vk::BufferCreateInfo staging_info;
	staging_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
	staging_info.size = texture_data.getBufferSize();
	staging_info.sharingMode = vk::SharingMode::eExclusive;
	vk::Buffer staging_buffer = device->createBuffer(staging_info);

	vk::MemoryRequirements staging_memory_request = device->getBufferMemoryRequirements(staging_buffer);
	vk::MemoryAllocateInfo staging_memory_alloc_info;
	staging_memory_alloc_info.allocationSize = staging_memory_request.size;
	staging_memory_alloc_info.memoryTypeIndex = gpu.getMemoryTypeIndex(staging_memory_request, vk::MemoryPropertyFlagBits::eHostVisible);

	vk::DeviceMemory staging_memory = device->allocateMemory(staging_memory_alloc_info);
	device->bindBufferMemory(staging_buffer, staging_memory, 0);

	auto* map = device->mapMemory(staging_memory, 0, texture_data.getBufferSize(), vk::MemoryMapFlags());
	memcpy(map, texture_data.m_data.data(), texture_data.getBufferSize());
	device->unmapMemory(staging_memory);

	vk::ImageSubresourceRange subresourceRange;
	subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.layerCount = 1;
	subresourceRange.levelCount = 1;

	vk::FenceCreateInfo fence_info;
	vk::FenceShared fence_shared;
	fence_shared.create(*device, fence_info);

	{
		// staging_bufferからimageへコピー
		// コマンドバッファの準備
		vk::CommandPool cmd_pool = sGlobal::Order().getCmdPoolTempolary(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.commandPool = cmd_pool;
		cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
		auto cmd = device->allocateCommandBuffers(cmd_buffer_info);

		cmd[0].reset(vk::CommandBufferResetFlags());
		vk::CommandBufferBeginInfo begin_info;
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
		cmd[0].begin(begin_info);

		vk::BufferImageCopy copy;
		copy.bufferOffset = 0;
		copy.imageExtent = { texture_data.m_size.x, texture_data.m_size.y, texture_data.m_size.z };
		copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageSubresource.mipLevel = 0;


		vk::ImageMemoryBarrier to_copy_barrier;
		to_copy_barrier.dstQueueFamilyIndex = device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
		to_copy_barrier.image = image;
		to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
		to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
		to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		to_copy_barrier.subresourceRange = subresourceRange;

		vk::ImageMemoryBarrier to_color_barrier;
		to_color_barrier.dstQueueFamilyIndex = device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
		to_color_barrier.image = image;
		to_color_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		to_color_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		to_color_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		to_color_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		to_color_barrier.subresourceRange = subresourceRange;

		cmd[0].pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
		cmd[0].copyBufferToImage(staging_buffer, image, vk::ImageLayout::eTransferDstOptimal, { copy });
		cmd[0].pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_color_barrier });
		cmd[0].end();
		vk::SubmitInfo submit_info;
		submit_info.commandBufferCount = (uint32_t)cmd.size();
		submit_info.pCommandBuffers = cmd.data();

		auto queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), device.getQueueNum(vk::QueueFlagBits::eGraphics) - 1);
		queue.submit(submit_info, *fence_shared);

		auto deleter = std::make_unique<Deleter>();
		deleter->device = device.getHandle();
		deleter->buffer = { staging_buffer };
		deleter->memory = { staging_memory };
		deleter->pool = cmd_pool;
		deleter->cmd = std::move(cmd);
		deleter->fence_shared = { fence_shared };
		sGlobal::Order().destroyResource(std::move(deleter));

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
	m_private->m_memory = memory;
	m_private->m_image_view = device->createImageView(view_info);
	m_private->m_sampler = device->createSampler(sampler_info);
	m_private->m_fence_shared = fence_shared;
}


std::vector<cModel::Material> loadMaterial(const aiScene* scene, const std::string& filename, vk::CommandBuffer cmd)
{
	auto device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_GRAPHICS];
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
			mat.mDiffuseTex.load(sThreadLocal::Order().m_gpu, device, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mAmbientTex.load(sThreadLocal::Order().m_gpu, device, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mSpecularTex.load(sThreadLocal::Order().m_gpu, device, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mNormalTex.load(sThreadLocal::Order().m_gpu, device, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mHeightTex.load(sThreadLocal::Order().m_gpu, device, sGlobal::Order().getThreadPool(), path + "/" + str.C_Str());
		}
	}

	return std::move(material);
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
	assert(cModel::NodeInfo::SUBMESH_NUM >= ainode->mNumMeshes); // メッシュを保存しておく容量が足りない
	assert(cModel::NodeInfo::CHILD_NUM >= ainode->mNumChildren); // 子供を保存しておく容量が足りない

	nodeBuffer.emplace_back();
	auto& node = nodeBuffer.back();
	node.mNodeNo = (s32)nodeBuffer.size() - 1;
	node.mParent = parentIndex;
//	node.mNodeName = std::hash<std::string>()(ainode->mName.C_Str());
	for (size_t i = 0; i < ainode->mNumChildren; i++) {
		loadNodeBufferRecurcive(ainode->mChildren[i], nodeBuffer, node.mNodeNo);
	}
}

std::vector<cModel::NodeInfo> loadNodeInfo(aiNode* ainode)
{
	assert(cModel::NodeInfo::SUBMESH_NUM >= ainode->mNumMeshes); // メッシュを保存しておく容量が足りない
	assert(cModel::NodeInfo::CHILD_NUM >= ainode->mNumChildren); // 子供を保存しておく容量が足りない

	std::vector<cModel::NodeInfo> nodeBuffer;
	nodeBuffer.reserve(countAiNode(ainode));
	nodeBuffer.emplace_back();
	auto& node = nodeBuffer.back();
	node.mNodeNo = (int32_t)nodeBuffer.size() - 1;
	node.mParent = -1;
//	node.mNodeName = std::hash<std::string>()(ainode->mName.C_Str());
	for (size_t i = 0; i < ainode->mNumChildren; i++) {
		loadNodeBufferRecurcive(ainode->mChildren[i], nodeBuffer, node.mNodeNo);
	}

	return nodeBuffer;
}

void loadMotion(cAnimation& anim_buffer, const aiScene* scene, const RootNode& root)
{
	if (!scene->HasAnimations()) {
		return;
	}
	const cGPU& gpu = sThreadLocal::Order().m_gpu;
	auto devices = gpu.getDevice(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
	auto share_family_index = gpu.getQueueFamilyIndexList(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);
	auto device = devices[0];

	std::vector<cModel::AnimationInfo> animation_info_list;
	std::vector<cModel::MotionInfo> motionInfoList;
	std::vector<cModel::MotionTimeBuffer> motionTimeBufferList;
	std::vector<cModel::MotionDataBuffer> motionDataBufferList;

	int dataCount = 0;
	int channnelCount = 0;
	for (size_t i = 0; i < scene->mNumAnimations; i++)
	{
		aiAnimation* anim = scene->mAnimations[i];
		for (size_t j = 0; j < anim->mNumChannels; j++)
		{
			aiNodeAnim* aiAnim = anim->mChannels[i];
			dataCount += aiAnim->mNumRotationKeys;
		}
		channnelCount += anim->mNumMeshChannels;
		motionInfoList.reserve(anim->mNumChannels);
	}
	animation_info_list.reserve(scene->mNumAnimations);
	motionInfoList.reserve(channnelCount);
	motionTimeBufferList.reserve(dataCount);
	motionDataBufferList.reserve(dataCount);

	for (size_t j = 0; j < scene->mNumAnimations; j++)
	{
		aiAnimation* anim = scene->mAnimations[j];

		for (size_t i = 0; i < anim->mNumChannels; i++)
		{
			aiNodeAnim* aiAnim = anim->mChannels[i];
			cModel::MotionInfo info;
			info.mNodeNo = root.getNodeIndexByName(aiAnim->mNodeName.C_Str());
			info.numData_ = aiAnim->mNumPositionKeys;
			info.offsetData_ = motionInfoList.empty() ? 0 : motionInfoList.back().offsetData_ + motionInfoList.back().numData_;
			motionInfoList.push_back(info);

			assert(aiAnim->mNumPositionKeys == aiAnim->mNumRotationKeys && aiAnim->mNumPositionKeys == aiAnim->mNumScalingKeys); // pos, rot, scaleは同じ数の場合のみGPUの計算に対応してる
			for (size_t n = 0; n < aiAnim->mNumPositionKeys; n++)
			{
				cModel::MotionDataBuffer d;
				float scale = aiAnim->mScalingKeys[n].mValue.x + aiAnim->mScalingKeys[n].mValue.y + aiAnim->mScalingKeys[n].mValue.z;
				scale /= 3.f; // 足して3で割る
				d.posAndScale_ = glm::vec4(aiAnim->mPositionKeys[n].mValue.x, aiAnim->mPositionKeys[n].mValue.y, aiAnim->mPositionKeys[n].mValue.z, scale);
				d.rot_ = glm::quat(aiAnim->mRotationKeys[n].mValue.w, aiAnim->mRotationKeys[n].mValue.x, aiAnim->mRotationKeys[n].mValue.y, aiAnim->mRotationKeys[n].mValue.z);
				motionDataBufferList.push_back(d);

				cModel::MotionTimeBuffer t;
				t.time_ = (float)aiAnim->mRotationKeys[n].mTime;
				motionTimeBufferList.push_back(t);
			}

		}
		cModel::AnimationInfo animation;
		animation.maxTime_ = (float)anim->mDuration;
		animation.ticksPerSecond_ = (float)anim->mTicksPerSecond;
		animation.numInfo_ = anim->mNumChannels;
		animation.offsetInfo_ = animation_info_list.empty() ? 0 : animation_info_list.back().offsetInfo_ + animation_info_list.back().numInfo_;
		animation_info_list.push_back(animation);

	}

	// AnimeInfo
	{
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::ANIMATION_INFO];
		buffer.create(gpu, device, animation_info_list, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	// MotionInfo
	{
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::MOTION_INFO];
		buffer.create(gpu, device, motionInfoList, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	// MotionTime
	{
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::MOTION_DATA_TIME];
		buffer.create(gpu, device, motionTimeBufferList, vk::BufferUsageFlagBits::eStorageBuffer);
	}
	// MotionInfo
	{
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::MOTION_DATA_SRT];
		buffer.create(gpu, device, motionDataBufferList, vk::BufferUsageFlagBits::eStorageBuffer);
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
		| aiProcess_Triangulate
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

	static VertexBuffer s_vertex_buffer(device, 128 * 65536);


	auto cmd_pool = sGlobal::Order().getCmdPoolTempolary(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));
	vk::CommandBufferAllocateInfo cmd_info;
	cmd_info.setCommandPool(cmd_pool);
	cmd_info.setLevel(vk::CommandBufferLevel::ePrimary);
	cmd_info.setCommandBufferCount(1);
	auto cmd = device->allocateCommandBuffers(cmd_info)[0];
	vk::CommandBufferBeginInfo begin_info;
	begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmd.begin(begin_info);

	// 初期化
	m_resource->mMeshNum = scene->mNumMeshes;
	m_resource->m_material = loadMaterial(scene, filename, cmd);

	m_resource->mNodeRoot = loadNode(scene);
	loadMotion(m_resource->m_animation_buffer, scene, m_resource->mNodeRoot);

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
	std::vector<Vertex> vertex;
	vertex.reserve(numVertex);
	std::vector<uint32_t> index;
	index.reserve(numIndex);


	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[i];
		materialIndex[i] = mesh->mMaterialIndex;

		// ELEMENT_ARRAY_BUFFER
		// 三角メッシュとして読み込む
		auto offset = (u32)vertex.size();
		for (u32 n = 0; n < mesh->mNumFaces; n++) {
			index.push_back(mesh->mFaces[n].mIndices[0] + offset);
			index.push_back(mesh->mFaces[n].mIndices[1] + offset);
			index.push_back(mesh->mFaces[n].mIndices[2] + offset);
		}

		// ARRAY_BUFFER
		std::vector<Vertex> _vertex(mesh->mNumVertices);
		for (size_t v = 0; v < mesh->mNumVertices; v++){
			_vertex[v].m_position = glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.f);
		}
		if (mesh->HasNormals()) {
			for (size_t v = 0; v < mesh->mNumVertices; v++) {
				_vertex[v].m_normal = glm::vec4(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z, 0.f);
			}
		}
		if (mesh->HasTextureCoords(0)) {
			for (size_t v = 0; v < mesh->mNumVertices; v++) {
				_vertex[v].m_texcoord0 = glm::vec4(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y, mesh->mTextureCoords[0][v].z, 0.f);
			}
		}
//		_vertex[i].mMaterialIndex = mesh->mMaterialIndex;

		// SkinMesh
		if (mesh->HasBones())
		{
			for (size_t b = 0; b < mesh->mNumBones; b++)
			{
				// BoneがMeshからしか参照できないので全部展開する
				int index = -1;
				for (size_t k = 0; k < boneList.size(); k++) {
					if (boneList[k].mName.compare(mesh->mBones[b]->mName.C_Str()) == 0) {
						index = (int)k;
						break;
					}

				}
				if (index == -1) {
					// 新しいボーンの登録
					Bone bone;
					bone.mName = mesh->mBones[b]->mName.C_Str();
					bone.mOffset = glm::transpose(aiTo(mesh->mBones[b]->mOffsetMatrix));
					bone.mNodeIndex = m_resource->mNodeRoot.getNodeIndexByName(mesh->mBones[b]->mName.C_Str());
					boneList.emplace_back(bone);
					index = (int)boneList.size() - 1;
					nodeInfo[bone.mNodeIndex].mBoneIndex = index;
				}

				for (size_t i = 0; i < mesh->mBones[b]->mNumWeights; i++)
				{
					aiVertexWeight& weight = mesh->mBones[b]->mWeights[i];
					Vertex& v = _vertex[weight.mVertexId];

					for (size_t o = 0; o < Vertex::BONE_NUM; o++) {
						if (v.m_bone_ID[o] == -1) {
							v.m_bone_ID[o] = index;
							v.m_weight[o] = weight.mWeight;
							break;
						}
					}
				}
			}
		}
		vertex.insert(vertex.end(), _vertex.begin(), _vertex.end());
	}
	importer.FreeScene();

	const cGPU& gpu = sThreadLocal::Order().m_gpu;
	auto familyIndex = device.getQueueFamilyIndex();

	{
		cMeshResource& mesh = m_resource->mMesh;
		mesh.mIndexType = vk::IndexType::eUint32;

		{
			mesh.m_vertex_buffer_ex = s_vertex_buffer.allocate(vector_sizeof(vertex));
			mesh.m_vertex_buffer_ex.update(vertex, 0);
			mesh.m_index_buffer_ex = s_vertex_buffer.allocate(vector_sizeof(index));
			mesh.m_index_buffer_ex.update(index, 0);
// 			mesh.m_vertex_buffer.create(gpu, device, vertex, vk::BufferUsageFlagBits::eVertexBuffer);
// 			mesh.m_index_buffer.create(gpu, device, index, vk::BufferUsageFlagBits::eIndexBuffer);
		}

		// indirect
		{
			std::vector<Mesh> indirect(indexSize.size());
			int offset = 0;
			for (size_t i = 0; i < indirect.size(); i++) {
				indirect[i].m_draw_cmd.indexCount = indexSize[i];
				indirect[i].m_draw_cmd.firstIndex = offset;
				indirect[i].m_draw_cmd.instanceCount = 0;
				indirect[i].m_draw_cmd.vertexOffset = 0;
				indirect[i].m_draw_cmd.firstInstance = 0;
				offset += indexSize[i];
			}
			mesh.mIndirectCount = (int32_t)indirect.size();
			mesh.m_indirect_buffer.create(gpu, device, indirect, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer);
		}
	}


	// vertex shader material
	{
		std::vector<VSMaterialBuffer> vsmb(m_resource->m_material.size());
		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::VS_MATERIAL);
		buffer.create(gpu, device, vsmb, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	// material
	{
		std::vector<MaterialBuffer> mb(m_resource->m_material.size());
		for (size_t i = 0; i < mb.size(); i++)
		{
			mb[i].mAmbient = m_resource->m_material[i].mAmbient;
			mb[i].mDiffuse = m_resource->m_material[i].mDiffuse;
			mb[i].mEmissive = m_resource->m_material[i].mEmissive;
			mb[i].mShininess = m_resource->m_material[i].mShininess;
			mb[i].mSpecular = m_resource->m_material[i].mSpecular;
		}
		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::MATERIAL);
		buffer.create(gpu, device, mb, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	// node info
	{
		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::NODE_INFO);
		buffer.create(gpu, device, nodeInfo, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	int instanceNum = 1000;
	if (!m_resource->mBone.empty())
	{
		// BoneInfo
		{
			std::vector<BoneInfo> bo(m_resource->mBone.size());
			for (size_t i = 0; i < m_resource->mBone.size(); i++) {
				bo[i].mBoneOffset = m_resource->mBone[i].mOffset;
				bo[i].mNodeIndex = m_resource->mBone[i].mNodeIndex;
			}
			size_t size = vector_sizeof(bo);
			auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::BONE_INFO);
			buffer.create(gpu, device, bo, vk::BufferUsageFlagBits::eStorageBuffer);
		}

		// BoneTransform
		{
			std::vector<BoneTransformBuffer> bt(m_resource->mBone.size() * instanceNum);
			auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::BONE_TRANSFORM);
			buffer.create(gpu, device, bt, vk::BufferUsageFlagBits::eStorageBuffer);
		}
	}



	// PlayingAnimation
	{
		std::vector<PlayingAnimation> pa(instanceNum);
		for (int i = 0; i < instanceNum; i++)
		{
			pa[i].playingAnimationNo = 0;
			pa[i].isLoop = true;
			pa[i].time = (float)(std::rand() % 200);
			pa[i].currentMotionInfoIndex = 0;
		}

		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::PLAYING_ANIMATION);
		buffer.create(gpu, device, pa, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	// MotionWork
	{
		std::vector<MotionWork> mw(m_resource->mNodeRoot.mNodeList.size()*instanceNum);
		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::MOTION_WORK);
		buffer.create(gpu, device, mw, vk::BufferUsageFlagBits::eStorageBuffer);

	}


	// ModelInfo
	{
		ModelInfo mi;
		mi.mInstanceMaxNum = instanceNum;
		mi.mInstanceAliveNum = 0;
		mi.mInstanceNum = 0;
		mi.mNodeNum = (s32)m_resource->mNodeRoot.mNodeList.size();
		mi.mBoneNum = (s32)m_resource->mBone.size();
		mi.mMeshNum = m_resource->mMeshNum;
		glm::vec3 max(-10e10f);
		glm::vec3 min(10e10f);
		for (auto& v : vertex)
		{
			max.x = glm::max(max.x, v.m_position.x);
			max.y = glm::max(max.y, v.m_position.y);
			max.z = glm::max(max.z, v.m_position.z);
			min.x = glm::min(min.x, v.m_position.x);
			min.y = glm::min(min.y, v.m_position.y);
			min.z = glm::min(min.z, v.m_position.z);
		}
		mi.mAabb = glm::vec4((max - min).xyz, glm::length((max - min) / 2.f));
		mi.mInvGlobalMatrix = glm::inverse(m_resource->mNodeRoot.getRootNode()->mTransformation);

		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::MODEL_INFO);
		buffer.create(gpu, device, sizeof(mi), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
		buffer.subupdate(&mi, sizeof(mi), 0);
		buffer.copyTo();
		m_resource->m_model_info = mi;
	}

	//BoneMap
	{
		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::BONE_MAP);
		buffer.create(gpu, device, instanceNum * sizeof(s32), vk::BufferUsageFlagBits::eStorageBuffer);
	}

	//	NodeLocalTransformBuffer
	{
		std::vector<NodeLocalTransformBuffer> nt(m_resource->mNodeRoot.mNodeList.size() * instanceNum);
		std::for_each(nt.begin(), nt.end(), [](NodeLocalTransformBuffer& v) { v.localAnimated_ = glm::mat4(1.f);  });

		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::NODE_LOCAL_TRANSFORM);
		buffer.create(gpu, device, nt, vk::BufferUsageFlagBits::eStorageBuffer);
	}


	//	NodeGlobalTransformBuffer
	{
		std::vector<NodeGlobalTransformBuffer> nt(m_resource->mNodeRoot.mNodeList.size() * instanceNum);
		std::for_each(nt.begin(), nt.end(), [](NodeGlobalTransformBuffer& v) { v.globalAnimated_ = glm::mat4(1.f);  });

		auto& buffer = m_resource->getBuffer(Resource::ModelConstantBuffer::NODE_GLOBAL_TRANSFORM);
		buffer.create(gpu, device, nt, vk::BufferUsageFlagBits::eStorageBuffer);
	}
	// world
	{
		auto& buffer = m_resource->getBuffer(Resource::ModelStorageBuffer::WORLD);
		buffer.create(gpu, device, sizeof(glm::mat4)*instanceNum, vk::BufferUsageFlagBits::eStorageBuffer);
	}


	m_resource->mVertexNum = std::move(vertexSize);
	m_resource->mIndexNum = std::move(indexSize);

	auto NodeNum = m_resource->mNodeRoot.mNodeList.size();
	auto BoneNum = m_resource->mBone.size();
	auto MeshNum = m_resource->mMeshNum;

	{
		int32_t local_size_x = 1024;
		// shaderのlocal_size_xと合わせる
		std::vector<glm::ivec3> group =
		{
			glm::ivec3(1, 1, 1),
			glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mNodeNum + local_size_x-1)/local_size_x, 1, 1),
			glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
			glm::ivec3((instanceNum + local_size_x - 1)/local_size_x, 1, 1),
			glm::ivec3((instanceNum*m_resource->m_model_info.mBoneNum + local_size_x - 1) / local_size_x, 1, 1),
		};

		m_resource->m_compute_indirect_buffer.create(gpu, device, group, vk::BufferUsageFlagBits::eIndirectBuffer);
	}

	cmd.end();
	auto queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), device.getQueueNum(vk::QueueFlagBits::eGraphics)-1);
	vk::SubmitInfo submit_info;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd;

	vk::FenceCreateInfo fence_info;
	vk::Fence fence = device->createFence(fence_info);

	queue.submit(submit_info, fence);
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
