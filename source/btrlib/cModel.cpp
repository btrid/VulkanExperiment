#include <btrlib/Define.h>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <atomic>

#include <btrlib/cModel.h>
#include <btrlib/sGlobal.h>
#include <btrlib/ThreadPool.h>
#include <btrlib/sDebug.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

namespace {
	int OREORE_PRESET = 0
		| aiProcess_JoinIdenticalVertices
		| aiProcess_ImproveCacheLocality
		| aiProcess_LimitBoneWeights
		| aiProcess_RemoveRedundantMaterials
		| aiProcess_SplitLargeMeshes
		| aiProcess_Triangulate
		;
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

	cModel::Texture loadTexture(const std::string& filename, vk::CommandBuffer& cmd)
	{

		auto device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_GRAPHICS];

		auto texture_data = rTexture::LoadTexture(filename);
		vk::ImageCreateInfo image_info;
		image_info.imageType = vk::ImageType::e2D;
		image_info.format = vk::Format::eR32G32B32A32Sfloat;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = vk::SampleCountFlagBits::e1;
//		image_info.tiling = vk::ImageTiling::eOptimal;
		image_info.tiling = vk::ImageTiling::eLinear;
		image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
		image_info.sharingMode = vk::SharingMode::eExclusive;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
		image_info.extent = { texture_data.m_size.x, texture_data.m_size.y, 1 };
		vk::Image image = device->createImage(image_info);

		vk::MemoryRequirements memory_request = device->getImageMemoryRequirements(image);
		vk::MemoryAllocateInfo memory_alloc_info;
		memory_alloc_info.allocationSize = memory_request.size;
		memory_alloc_info.memoryTypeIndex = sThreadLocal::Order().m_gpu.getMemoryTypeIndex(memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

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
		staging_memory_alloc_info.memoryTypeIndex = sThreadLocal::Order().m_gpu.getMemoryTypeIndex(staging_memory_request, vk::MemoryPropertyFlagBits::eHostVisible);

		vk::DeviceMemory staging_memory = device->allocateMemory(staging_memory_alloc_info);
		device->bindBufferMemory(staging_buffer, staging_memory, 0);

		auto* map = device->mapMemory(staging_memory, 0, texture_data.getBufferSize(), vk::MemoryMapFlags());
		memcpy(map, texture_data.m_data.data(), texture_data.getBufferSize());
		device->unmapMemory(staging_memory);

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
//		sampler_info.unnormalizedCoordinates
		vk::Sampler sampler = device->createSampler(sampler_info);

		cModel::Texture texture;
		texture.m_image = image;
		texture.m_memory = memory;
		texture.m_sampler = sampler;

// 		vk::FenceCreateInfo fence_info;
// 		fence_info.setFlags(vk::FenceCreateFlagBits::eSignaled);
// 		texture.m_fence = device->createFence(fence_info);

		vk::BufferImageCopy copy;
		copy.bufferOffset = 0;
		copy.imageExtent = {texture_data.m_size.x, texture_data.m_size.y, texture_data.m_size.z};
		copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageSubresource.mipLevel = 0;

		vk::ImageMemoryBarrier to_copy_barrier;
		to_copy_barrier.dstQueueFamilyIndex = device.getQueueFamilyIndex();
		to_copy_barrier.image = image;
		to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
		to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
		to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		to_copy_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		to_copy_barrier.subresourceRange.baseArrayLayer = 0;
		to_copy_barrier.subresourceRange.baseMipLevel = 0;
		to_copy_barrier.subresourceRange.layerCount = 1;
		to_copy_barrier.subresourceRange.levelCount = 1;

		vk::ImageMemoryBarrier to_color_barrier;
		to_color_barrier.dstQueueFamilyIndex = device.getQueueFamilyIndex();
		to_color_barrier.image = image;
		to_color_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		to_color_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		to_color_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		to_color_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		to_color_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		to_color_barrier.subresourceRange.baseArrayLayer = 0;
		to_color_barrier.subresourceRange.baseMipLevel = 0;
		to_color_barrier.subresourceRange.layerCount = 1;
		to_color_barrier.subresourceRange.levelCount = 1;

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
		cmd.copyBufferToImage(staging_buffer, image, vk::ImageLayout::eTransferDstOptimal, { copy });
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_color_barrier });

		vk::ImageViewCreateInfo view_info;
		view_info.viewType = vk::ImageViewType::e2D;
		view_info.components.r = vk::ComponentSwizzle::eR;
		view_info.components.g = vk::ComponentSwizzle::eG;
		view_info.components.b = vk::ComponentSwizzle::eB;
		view_info.components.a = vk::ComponentSwizzle::eA;
		view_info.flags = vk::ImageViewCreateFlags();
		view_info.format = vk::Format::eR32G32B32A32Sfloat;
		view_info.image = image;
		view_info.subresourceRange = to_color_barrier.subresourceRange;
		texture.m_image_view = device->createImageView(view_info);
		return std::move(texture);
	}

}


std::vector<cModel::Material> loadMaterial(const aiScene* scene, const std::string& filename, vk::CommandBuffer cmd)
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
			mat.mDiffuseTex = loadTexture(path + "/" + str.C_Str(), cmd);
		}
		if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mAmbientTex = loadTexture(path + "/" + str.C_Str(), cmd);
		}
		if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mSpecularTex = loadTexture(path + "/" + str.C_Str(), cmd);
		}

		if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mNormalTex = loadTexture(path + "/" + str.C_Str(), cmd);
		}

		if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mHeightTex = loadTexture(path + "/" + str.C_Str(), cmd);
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
	: mPrivate(std::make_shared<Private>())
{
	{
		//		Frustom frustom;
		//		frustom.setCamera(Renderer::order()->camera());

		//		glCreateBuffers(1, &frustomBO_);
		//		glNamedBufferStorage(frustomBO_, sizeof(Plane) * 6, frustom.getPlane().data(), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);

	}


}
cModel::~cModel()
{}


void cModel::load(const std::string& filename)
{
	mPrivate->mFilename = filename;

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filename, OREORE_PRESET);
	if (!scene) {
		sDebug::Order().print(sDebug::FLAG_ERROR | sDebug::ACTION_ASSERTION,"can't file load in cModel::load : %s : %s\n", filename.c_str(), importer.GetErrorString());
		return;
	}

	auto s = std::chrono::system_clock::now();

	auto device = sThreadLocal::Order().m_device[sThreadLocal::DEVICE_GRAPHICS];
	auto cmd_pool = sGlobal::Order().getCmdPoolTempolary(device.getQueueFamilyIndex());
	vk::CommandBufferAllocateInfo cmd_info;
	cmd_info.setCommandPool(cmd_pool);
	cmd_info.setLevel(vk::CommandBufferLevel::ePrimary);
	cmd_info.setCommandBufferCount(1);
	auto cmd = device->allocateCommandBuffers(cmd_info)[0];
	vk::CommandBufferBeginInfo begin_info;
	begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmd.begin(begin_info);

	// 初期化
	mPrivate->mMeshNum = scene->mNumMeshes;
	mPrivate->m_material = loadMaterial(scene, filename, cmd);

	mPrivate->mNodeRoot = loadNode(scene);
	loadMotion(mPrivate->m_animation_buffer, scene, mPrivate->mNodeRoot);

	std::vector<NodeInfo> nodeInfo = loadNodeInfo(scene->mRootNode);

	std::vector<Bone>& boneList = mPrivate->mBone;

	mPrivate->m_material_index.resize(scene->mNumMeshes);
	unsigned numIndex = 0;
	unsigned numVertex = 0;
	std::vector<int> vertexSize(scene->mNumMeshes);
	std::vector<int> indexSize(scene->mNumMeshes);
	std::vector<int>& materialIndex = mPrivate->m_material_index;
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
					bone.mNodeIndex = mPrivate->mNodeRoot.getNodeIndexByName(mesh->mBones[b]->mName.C_Str());
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
		cMeshGPU& mesh = mPrivate->mMesh;
		mesh.mIndexType = vk::IndexType::eUint32;
		mesh.mBufferSize[0] = vector_sizeof(vertex);
		mesh.mBufferSize[1] = vector_sizeof(index);

		{
			int bufferSize
				= mesh.mBufferSize[0]
				+ mesh.mBufferSize[1];
			vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
				.setUsage(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer /*| vk::BufferUsageFlagBits::eTransferDst*/)
				.setSize(bufferSize);

			mesh.mBuffer = device->createBuffer(bufferInfo);

			vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(mesh.mBuffer);

			vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
				.setAllocationSize(memoryRequest.size)
				.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, /*vk::MemoryPropertyFlagBits::eDeviceLocal*/ vk::MemoryPropertyFlagBits::eHostVisible));
			mesh.mMemory = device->allocateMemory(memAlloc);

			char* mem = reinterpret_cast<char*>(device->mapMemory(mesh.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
			{
				int offset = 0;
				int size = mesh.mBufferSize[0];
				memcpy_s(mem + offset, size, vertex.data(), size);

				offset += size;
				size = mesh.mBufferSize[1];
				memcpy_s(mem + offset, size, index.data(), size);

			}
			device->unmapMemory(mesh.mMemory);
			device->bindBufferMemory(mesh.mBuffer, mesh.mMemory, 0);
		}

		// indirect
		{
			std::vector<Mesh> indirect(indexSize.size());
			int offset = 0;
			for (size_t i = 0; i < indirect.size(); i++) {
				indirect[i].m_draw_cmd.indexCount = indexSize[i];
				indirect[i].m_draw_cmd.firstIndex = offset;
				indirect[i].m_draw_cmd.instanceCount = 1;
				indirect[i].m_draw_cmd.vertexOffset = 0;
				indirect[i].m_draw_cmd.firstInstance = 0;
				offset += indexSize[i];
			}
			mesh.mIndirectCount = (int32_t)indirect.size();
			mesh.mBufferSize[2] = (int32_t)vector_sizeof(indirect);
			s32 bufferSize = mesh.mBufferSize[2];
			{
				vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
					.setUsage(vk::BufferUsageFlagBits::eIndirectBuffer
						| vk::BufferUsageFlagBits::eStorageBuffer
						| vk::BufferUsageFlagBits::eTransferDst)
					.setSize(bufferSize);
					mesh.mBufferIndirect = device->createBuffer(bufferInfo);
				vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(mesh.mBufferIndirect);

				vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
					.setAllocationSize(memoryRequest.size)
					.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
				mesh.mMemoryIndirect = device->allocateMemory(memAlloc);

				auto* mem = reinterpret_cast<Mesh*>(device->mapMemory(mesh.mMemoryIndirect, 0, memoryRequest.size, vk::MemoryMapFlags()));
				{
					memcpy_s(mem, bufferSize, indirect.data(), bufferSize);
				}
				device->unmapMemory(mesh.mMemoryIndirect);
				device->bindBufferMemory(mesh.mBufferIndirect, mesh.mMemoryIndirect, 0);

				mesh.mIndirectInfo.setBuffer(mesh.mBufferIndirect);
				mesh.mIndirectInfo.setOffset(0);
				mesh.mIndirectInfo.setRange(mesh.mBufferSize[2]);

			}

		}
	}


	// vertex shader material
	{
		std::vector<VSMaterialBuffer> vsmb(mPrivate->m_material.size());
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::VS_MATERIAL);
		buffer.create(gpu, device, vsmb, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	// material
	{
		std::vector<MaterialBuffer> mb(mPrivate->m_material.size());
		for (size_t i = 0; i < mb.size(); i++)
		{
			mb[i].mAmbient = mPrivate->m_material[i].mAmbient;
			mb[i].mDiffuse = mPrivate->m_material[i].mDiffuse;
			mb[i].mEmissive = mPrivate->m_material[i].mEmissive;
			mb[i].mShininess = mPrivate->m_material[i].mShininess;
			mb[i].mSpecular = mPrivate->m_material[i].mSpecular;
			//			mb[i].mAmbientTex = material[i].mAmbientTex.makeBindless();
			//			mb[i].mDiffuseTex = material[i].mDiffuseTex.makeBindless();
			//			mb[i].mNormalTex = material[i].mNormalTex.makeBindless();
			//			mb[i].mHeightTex = material[i].mHeightTex.makeBindless();
			//			mb[i].mSpecularTex = material[i].mSpecularTex.makeBindless();
		}
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::MATERIAL);
		buffer.create(gpu, device, mb, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	// node info
	{
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO);
		buffer.create(gpu, device, nodeInfo, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	int instanceNum = 1;
	if (!mPrivate->mBone.empty())
	{
		// BoneInfo
		{
			std::vector<BoneInfo> bo(mPrivate->mBone.size());
			for (size_t i = 0; i < mPrivate->mBone.size(); i++) {
				bo[i].mBoneOffset = mPrivate->mBone[i].mOffset;
				bo[i].mNodeIndex = mPrivate->mBone[i].mNodeIndex;
			}
			size_t size = vector_sizeof(bo);
			auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::BONE_INFO);
			buffer.create(gpu, device, bo, vk::BufferUsageFlagBits::eStorageBuffer);
		}

		// BoneTransform
		{
			std::vector<BoneTransformBuffer> bt(mPrivate->mBone.size() * instanceNum);
			auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM);
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

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::PLAYING_ANIMATION);
		buffer.create(gpu, device, pa, vk::BufferUsageFlagBits::eStorageBuffer);

// 		vk::DebugMarkerObjectNameInfoEXT debug_info;
// 		debug_info.object = (uint64_t)(VkBuffer)buffer.mBuffer;
// 		debug_info.objectType = vk::DebugReportObjectTypeEXT::eBuffer;
// 		debug_info.pObjectName = "PlayingAnimation";
// 		device.DebugMarkerSetObjectName(&debug_info);

	}

	// MotionWork
	{
		std::vector<MotionWork> mw(mPrivate->mNodeRoot.mNodeList.size()*instanceNum);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::MOTION_WORK);
		buffer.create(gpu, device, mw, vk::BufferUsageFlagBits::eStorageBuffer);

	}


	// ModelInfo
	{
		std::vector<ModelInfo> mi(1);
		mi[0].mInstanceMaxNum = instanceNum;
		mi[0].mInstanceNum = instanceNum;
		mi[0].mNodeNum = mPrivate->mNodeRoot.mNodeList.size();
		mi[0].mBoneNum = mPrivate->mBone.size();
		mi[0].mMeshNum = mPrivate->mMeshNum;
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
		mi[0].mAabb = glm::vec4((max - min).xyz, glm::length((max - min) / 2.f));
		mi[0].mInvGlobalMatrix = glm::inverse(mPrivate->mNodeRoot.getRootNode()->mTransformation);

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO);
		buffer.create(gpu, device, mi, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer);
	}

	//BoneMap
	{
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
			.setSize(instanceNum * sizeof(s32))
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::BONE_MAP);
		buffer.m_buffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.m_buffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eDeviceLocal));
		buffer.m_memory = device->allocateMemory(memAlloc);
		device->bindBufferMemory(buffer.m_buffer, buffer.m_memory, 0);

		buffer.m_buffer_info.setBuffer(buffer.m_buffer);
		buffer.m_buffer_info.setOffset(0);
		buffer.m_buffer_info.setRange(instanceNum * sizeof(s32));
	}

	//	NodeLocalTransformBuffer
	{
		std::vector<NodeLocalTransformBuffer> nt(mPrivate->mNodeRoot.mNodeList.size() * instanceNum);
		std::for_each(nt.begin(), nt.end(), [](NodeLocalTransformBuffer& v) { v.localAnimated_ = glm::mat4(1.f);  });

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::NODE_LOCAL_TRANSFORM);
		buffer.create(gpu, device, nt, vk::BufferUsageFlagBits::eStorageBuffer);
	}


	//	NodeGlobalTransformBuffer
	{
		std::vector<NodeGlobalTransformBuffer> nt(mPrivate->mNodeRoot.mNodeList.size() * instanceNum);
		std::for_each(nt.begin(), nt.end(), [](NodeGlobalTransformBuffer& v) { v.globalAnimated_ = glm::mat4(1.f);  });

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::NODE_GLOBAL_TRANSFORM);
		buffer.create(gpu, device, nt, vk::BufferUsageFlagBits::eStorageBuffer);
	}
	// world
	{
		std::vector<glm::mat4> world(instanceNum);

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::WORLD);
		buffer.create(gpu, device, world, vk::BufferUsageFlagBits::eStorageBuffer);

	}


	mPrivate->mVertexNum = std::move(vertexSize);
	mPrivate->mIndexNum = std::move(indexSize);

	auto NodeNum = mPrivate->mNodeRoot.mNodeList.size();
	auto BoneNum = mPrivate->mBone.size();
	auto MeshNum = mPrivate->mMeshNum;

	{
		std::vector<glm::ivec3> group =
		{
			glm::ivec3(1000 / 256 + 1, 1, 1),
			glm::ivec3(256, 1, 1),
			glm::ivec3(256, 1, 1),
			glm::ivec3(256, 1, 1),
			glm::ivec3(256, 1, 1),
			glm::ivec3(256, 1, 1),
		};
		size_t size = vector_sizeof(group);

		vk::BufferCreateInfo buffer_info = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer)
// 			.setQueueFamilyIndexCount(1)
// 			.setPQueueFamilyIndices(&device.getQueueFamilyIndex())
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);

		mPrivate->m_compute_indirect_buffer = device->createBuffer(buffer_info);
		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(mPrivate->m_compute_indirect_buffer);
		vk::MemoryAllocateInfo memoryAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		mPrivate->m_compute_indirect_memory = device->allocateMemory(memoryAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(mPrivate->m_compute_indirect_memory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, group.data(), size);
		}
		device->unmapMemory(mPrivate->m_compute_indirect_memory);
		device->bindBufferMemory(mPrivate->m_compute_indirect_buffer, mPrivate->m_compute_indirect_memory, 0);
		mPrivate->m_compute_indirect_buffer_info
			.setBuffer(mPrivate->m_compute_indirect_buffer)
			.setOffset(0)
			.setRange(memoryRequest.size);
	}

	{
		auto cmd_pool = sThreadLocal::Order().getCmdPoolCompiled(device.getQueueFamilyIndex())[0];
		//		for (size_t i = 0; i < cmd_pool.size(); i++)
		{
			// present barrier cmd
			vk::CommandBufferAllocateInfo cmd_info = vk::CommandBufferAllocateInfo()
				.setCommandPool(cmd_pool)
				.setLevel(vk::CommandBufferLevel::ePrimary)
				.setCommandBufferCount(2);
			m_cmd_graphics = device->allocateCommandBuffers(cmd_info);
		}
	}

	cmd.end();
	auto queue = device->getQueue(device.getQueueFamilyIndex(), device.getQueueNum()-1);
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
	deleter->fence = fence;
	sGlobal::Order().destroyResource(std::move(deleter));

	auto e = std::chrono::system_clock::now();
	auto t = std::chrono::duration_cast<std::chrono::microseconds>(e - s);
	sDebug::Order().print(sDebug::FLAG_LOG | sDebug::SOURCE_MODEL, "[Load Complete %6.2fs] %s NodeNum = %d BoneNum = %d \n", t.count() / 1000.f / 1000.f, filename.c_str(), NodeNum, BoneNum);

}


const std::string& cModel::getFilename() const
{
	return mPrivate->mFilename;
}
const cMeshGPU& cModel::getMesh() const
{
	return mPrivate->mMesh; 
}
