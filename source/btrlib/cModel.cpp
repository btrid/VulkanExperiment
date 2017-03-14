#include <btrlib/Define.h>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <atomic>

#include <btrlib/cModel.h>
#include <btrlib/sVulkan.h>
#include <btrlib/ThreadPool.h>
#include <btrlib/sDebug.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

#define OREORE_PRESET ( \
	/*aiProcess_CalcTangentSpace				| */ \
	/*aiProcess_GenSmoothNormals				| */ \
	aiProcess_JoinIdenticalVertices			|  \
	aiProcess_ImproveCacheLocality			|  \
	aiProcess_LimitBoneWeights				|  \
	aiProcess_RemoveRedundantMaterials      |  \
	aiProcess_SplitLargeMeshes				|  \
	aiProcess_Triangulate					|  \
	/*aiProcess_GenUVCoords                   | */ \
	aiProcess_SortByPType                   |  \
	aiProcess_FindDegenerates               |  \
	aiProcess_FlipUVs						|  \
	0 )

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


std::vector<cModel::Material> loadMaterial(const aiScene* scene, const std::string& filename)
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
		aiReturn isSuccess = aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode);
		/*			if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
		mat.mDiffuseTex.load(path + "/" + str.C_Str());
		}
		isSuccess = aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode);
		if (isSuccess == AI_SUCCESS) {
		mat.mAmbientTex.load(path + "/" + str.C_Str());
		}
		isSuccess = aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode);
		if (isSuccess == AI_SUCCESS) {
		mat.mSpecularTex.load(path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
		mat.mNormalTex.load(path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
		mat.mHeightTex.load(path + "/" + str.C_Str());
		}
		*/
	}

	return std::move(material);
}

void _loadNodeRecurcive(aiNode* ainode, RootNode& root, int parent)
{
	auto nodeIndex = root.mNodeList.size();
	Node node;
	node.mName = ainode->mName.C_Str();
	node.mTransformation = AI_TO(ainode->mTransformation);
	node.mParent = parent;
	if (parent >= 0) {
		root.mNodeList[parent].mChildren.push_back(nodeIndex);
	}
	root.mNodeList.push_back(node);

	for (size_t i = 0; i < ainode->mNumChildren; i++) {
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
	node.mNodeNo = nodeBuffer.size() - 1;
	node.mParent = parentIndex;
	node.mNodeName = std::hash<std::string>()(ainode->mName.C_Str());
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
	node.mNodeNo = nodeBuffer.size() - 1;
	node.mParent = -1;
	node.mNodeName = std::hash<std::string>()(ainode->mName.C_Str());
	for (size_t i = 0; i < ainode->mNumChildren; i++) {
		loadNodeBufferRecurcive(ainode->mChildren[i], nodeBuffer, node.mNodeNo);
	}

	return nodeBuffer;
}

cAnimation loadMotion(const aiScene* scene, const RootNode& root)
{
	cAnimation anim_buffer;
	if (!scene->HasAnimations()) {
		return anim_buffer;
	}
	const cGPU& gpu = sThreadData::Order().m_gpu;
	auto devices = gpu.getDevice(vk::QueueFlagBits::eTransfer);
	auto familyIndex = gpu.getQueueFamilyIndexList(vk::QueueFlagBits::eTransfer);
	auto device = devices[0];

	std::vector<cModel::AnimationInfo> animationInfoList;
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
	animationInfoList.reserve(scene->mNumAnimations);
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
																																 // MotionBuffer
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
		animation.offsetInfo_ = animationInfoList.empty() ? 0 : animationInfoList.back().offsetInfo_ + animationInfoList.back().numInfo_;
		animationInfoList.push_back(animation);

	}

	// AnimeInfo
	{
		auto size = vector_sizeof(animationInfoList);
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::ANIMATION_INFO];
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memoryAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memoryAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, animationInfoList.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);

	}

	// MotionInfo
	{
		auto size = vector_sizeof(motionInfoList);
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::MOTION_INFO];
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memoryAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memoryAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, motionInfoList.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
	}

	// MotionTime
	{
		auto size = vector_sizeof(motionTimeBufferList);
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::MOTION_DATA_TIME];
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memoryAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memoryAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, motionTimeBufferList.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);

	}
	// MotionInfo
	{
		auto size = vector_sizeof(motionDataBufferList);
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = anim_buffer.mMotionBuffer[cAnimation::MOTION_DATA_SRT];
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memoryAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memoryAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, motionDataBufferList.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);

	}

	return anim_buffer;
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

	auto s = std::chrono::system_clock::now();

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filename, OREORE_PRESET);
	if (!scene) {
		sDebug::Order().print(sDebug::FLAG_ERROR | sDebug::ACTION_ASSERTION,"can't file load in cModel::load : %s : %s\n", filename.c_str(), importer.GetErrorString());
		return;
	}

	// 初期化
	mPrivate->mMeshNum = scene->mNumMeshes;
	std::vector<Material> material = loadMaterial(scene, filename);

	mPrivate->mNodeRoot = loadNode(scene);
	mPrivate->m_animation_buffer = loadMotion(scene, mPrivate->mNodeRoot);

	std::vector<NodeInfo> nodeInfo = loadNodeInfo(scene->mRootNode);

	std::vector<Bone>& boneList = mPrivate->mBone;

	unsigned numIndex = 0;
	unsigned numVertex = 0;
	std::vector<int> vertexSize(scene->mNumMeshes);
	std::vector<int> indexSize(scene->mNumMeshes);
	std::vector<int> materialIndex(scene->mNumMeshes);
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
		auto offset = vertex.size();
		for (size_t n = 0; n < mesh->mNumFaces; n++) {
			index.push_back(mesh->mFaces[n].mIndices[0] + offset);
			index.push_back(mesh->mFaces[n].mIndices[1] + offset);
			index.push_back(mesh->mFaces[n].mIndices[2] + offset);
		}

		// ARRAY_BUFFER
		std::vector<Vertex> _vertex(mesh->mNumVertices);
		for (size_t i = 0; i < mesh->mNumVertices; i++)
		{
			_vertex[i].mPosition = glm::vec4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.f);
			if (mesh->HasNormals()) {
				_vertex[i].mNormal = glm::vec4(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z, 0.f);
			}
			if (mesh->HasTextureCoords(0)) {
				_vertex[i].mTexcoord0 = glm::vec4(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y, mesh->mTextureCoords[0][i].z, 0.f);
			}
			_vertex[i].mMaterialIndex = mesh->mMaterialIndex;
		}

		// SkinMesh
		if (mesh->HasBones())
		{
			for (size_t b = 0; b < mesh->mNumBones; b++)
			{
				// BoneがMeshからしか参照できないので全部展開する
				int index = -1;
				for (size_t k = 0; k < boneList.size(); k++) {
					if (boneList[k].mName.compare(mesh->mBones[b]->mName.C_Str()) == 0) {
						index = k;
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
					index = boneList.size() - 1;
					nodeInfo[bone.mNodeIndex].mBoneIndex = index;
				}

				for (size_t i = 0; i < mesh->mBones[b]->mNumWeights; i++)
				{
					aiVertexWeight& weight = mesh->mBones[b]->mWeights[i];
					Vertex& v = _vertex[weight.mVertexId];

					for (size_t o = 0; o < Vertex::BONE_NUM; o++) {
						if (v.boneID_[o] == -1) {
							v.boneID_[o] = index;
							v.weight_[o] = weight.mWeight;
							break;
						}
					}
				}
			}
		}
		vertex.insert(vertex.end(), _vertex.begin(), _vertex.end());
	}


	const cGPU& gpu = sThreadData::Order().m_gpu;
	auto device = sThreadData::Order().m_device[sThreadData::DEVICE_GRAPHICS];
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

			{

				// 頂点の設定
				mesh.mBinding =
				{
					vk::VertexInputBindingDescription()
					.setBinding(0)
					.setInputRate(vk::VertexInputRate::eVertex)
					.setStride(sizeof(Vertex))
				};

				mesh.mAttribute =
				{
					// pos
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(0)
					.setFormat(vk::Format::eR32G32B32A32Sfloat)
					.setOffset(0),
					// normal
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(1)
					.setFormat(vk::Format::eR32G32B32A32Sfloat)
					.setOffset(16),
					// texcoord
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(2)
					.setFormat(vk::Format::eR32G32B32A32Sfloat)
					.setOffset(32),
					// boneID
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(3)
					.setFormat(vk::Format::eR32G32B32A32Sint)
					.setOffset(48),
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(4)
					.setFormat(vk::Format::eR32G32B32A32Sfloat)
					.setOffset(64),
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(5)
					.setFormat(vk::Format::eR32Sint)
					.setOffset(80),
				};
				mesh.mVertexInfo = vk::PipelineVertexInputStateCreateInfo()
					.setVertexBindingDescriptionCount(mesh.mBinding.size())
					.setPVertexBindingDescriptions(mesh.mBinding.data())
					.setVertexAttributeDescriptionCount(mesh.mAttribute.size())
					.setPVertexAttributeDescriptions(mesh.mAttribute.data());

			}
		}

		// indirect
		{
			std::vector<Mesh> indirect =
			{
				Mesh(
					vk::DrawIndexedIndirectCommand()
					.setIndexCount((uint32_t)index.size())
					.setInstanceCount(1u)
				),
			};
			mesh.mIndirectCount = indirect.size();
			mesh.mBufferSize[2] = vector_sizeof(indirect);
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
					.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::/*eDeviceLocal*/eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
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


	/*	for (unsigned num_vertex = 0; num_vertex < _vertexSize.size(); num_vertex++)
	{
	AABB aabb;
	for (int vi = 0; vi < _vertexSize[num_vertex]; vi++)
	{
	aabb.max_.x = glm::max(aabb.max_.x, _vertex[vi].mPosition.x);
	aabb.max_.y = glm::max(aabb.max_.y, _vertex[vi].mPosition.y);
	aabb.max_.z = glm::max(aabb.max_.z, _vertex[vi].mPosition.z);
	aabb.min_.x = glm::min(aabb.min_.x, _vertex[vi].mPosition.x);
	aabb.min_.y = glm::min(aabb.min_.y, _vertex[vi].mPosition.y);
	aabb.min_.z = glm::min(aabb.min_.z, _vertex[vi].mPosition.z);
	}
	command[num_vertex].AABB_ = glm::vec4((aabb.max_ - aabb.min_) / 2.f, glm::length((aabb.max_ - aabb.min_) / 2.f));
	command[num_vertex].numElement_ = _indexSize[num_vertex];
	command[num_vertex].numVertex_ = _vertexSize[num_vertex];
	}
	*/
	//	mPrivate->setNodeIndex(command, scene->mRootNode);

	// vertex shader material
	{
		std::vector<VSMaterialBuffer> vsmb(material.size());
		for (size_t i = 0; i < vsmb.size(); i++)
		{
			//			vsmb[i].normalTex_ = material[i].ormalTex_.makeBindless();
			//			vsmb[i].heightTex_ = material[i].heightTex_.makeBindless();
		}
		auto size = vector_sizeof(vsmb);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::VS_MATERIAL);
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);

		buffer.mBuffer = device->createBuffer(bufferInfo);
		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
	}

	// material
	{
		std::vector<MaterialBuffer> mb(material.size());
		for (size_t i = 0; i < mb.size(); i++)
		{
			mb[i].mAmbient = material[i].mAmbient;
			mb[i].mDiffuse = material[i].mDiffuse;
			mb[i].mEmissive = material[i].mEmissive;
			mb[i].mShininess = material[i].mShininess;
			mb[i].mSpecular = material[i].mSpecular;
			//			mb[i].mAmbientTex = material[i].mAmbientTex.makeBindless();
			//			mb[i].mDiffuseTex = material[i].mDiffuseTex.makeBindless();
			//			mb[i].mNormalTex = material[i].mNormalTex.makeBindless();
			//			mb[i].mHeightTex = material[i].mHeightTex.makeBindless();
			//			mb[i].mSpecularTex = material[i].mSpecularTex.makeBindless();
		}
		size_t size = vector_sizeof(mb);

		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::MATERIAL);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, mb.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
	}

	// node info
	{
		size_t size = vector_sizeof(nodeInfo);
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
// 			.setQueueFamilyIndexCount(1)
// 			.setPQueueFamilyIndices(&device.getQueueFamilyIndex())
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, nodeInfo.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
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
			vk::BufferCreateInfo boneBufferInfo = vk::BufferCreateInfo()
				.setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
				.setSize(size)
				.setSharingMode(vk::SharingMode::eExclusive);

			auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::BONE_INFO);
			buffer.mBuffer = device->createBuffer(boneBufferInfo);

			vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
			vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
				.setAllocationSize(memoryRequest.size)
				.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
			buffer.mMemory = device->allocateMemory(memAlloc);

			char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
			{
				memcpy_s(mem, size, bo.data(), size);
			}
			device->unmapMemory(buffer.mMemory);
			device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

			buffer.mBufferInfo.setBuffer(buffer.mBuffer);
			buffer.mBufferInfo.setOffset(0);
			buffer.mBufferInfo.setRange(size);
		}

		// BoneTransform
		{
			std::vector<BoneTransformBuffer> bt(mPrivate->mBone.size() * instanceNum);
			size_t size = vector_sizeof(bt);
			vk::BufferCreateInfo boneBufferInfo = vk::BufferCreateInfo()
				.setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
				.setSize(vector_sizeof(bt))
				.setSharingMode(vk::SharingMode::eExclusive);

			auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM);
			buffer.mBuffer = device->createBuffer(boneBufferInfo);

			vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
			vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
				.setAllocationSize(memoryRequest.size)
				.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
			buffer.mMemory = device->allocateMemory(memAlloc);

			char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
			{
				memcpy_s(mem, size, bt.data(), size);
			}
			device->unmapMemory(buffer.mMemory);
			device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

			buffer.mBufferInfo.setBuffer(buffer.mBuffer);
			buffer.mBufferInfo.setOffset(0);
			buffer.mBufferInfo.setRange(size);

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
		size_t size = vector_sizeof(pa);

		vk::BufferCreateInfo bufferInfo;
		bufferInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
		bufferInfo.setSize(size);
		bufferInfo.setSharingMode(vk::SharingMode::eExclusive);

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::PLAYING_ANIMATION);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, pa.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);

	}

	// MotionWork
	{
		std::vector<MotionWork> mw(mPrivate->mNodeRoot.mNodeList.size()*instanceNum);
		size_t size = vector_sizeof(mw);
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::MOTION_WORK);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, mw.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
	}


	// ModelInfo
	{
		ModelInfo mi;
		mi.mInstanceMaxNum = instanceNum;
		mi.mInstanceNum = instanceNum;
		mi.mNodeNum = mPrivate->mNodeRoot.mNodeList.size();
		mi.mBoneNum = mPrivate->mBone.size();
		mi.mMeshNum = mPrivate->mMeshNum;
		glm::vec3 max(-10e10f);
		glm::vec3 min(10e10f);
		for (auto& v : vertex)
		{
			max.x = glm::max(max.x, v.mPosition.x);
			max.y = glm::max(max.y, v.mPosition.y);
			max.z = glm::max(max.z, v.mPosition.z);
			min.x = glm::min(min.x, v.mPosition.x);
			min.y = glm::min(min.y, v.mPosition.y);
			min.z = glm::min(min.z, v.mPosition.z);
		}

		mi.mAabb = glm::vec4((max - min).xyz, glm::length((max - min) / 2.f));
		mi.mInvGlobalMatrix = AI_TO(scene->mRootNode->mTransformation.Inverse());

		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)
// 			.setQueueFamilyIndexCount(1)
// 			.setPQueueFamilyIndices(&device.getQueueFamilyIndex())
			.setSize(sizeof(mi))
			.setSharingMode(vk::SharingMode::eExclusive);

		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(sizeof(mi));
	}

	//BoneMap
	{
		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
// 			.setQueueFamilyIndexCount(1)
// 			.setPQueueFamilyIndices(&device.getQueueFamilyIndex())
			.setSize(instanceNum * sizeof(s32))
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::BONE_MAP);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			//			.memoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eDeviceLocal));
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(instanceNum * sizeof(s32));
	}

	//	NodeLocalTransformBuffer
	{
		std::vector<NodeLocalTransformBuffer> nt(mPrivate->mNodeRoot.mNodeList.size() * instanceNum);
		std::for_each(nt.begin(), nt.end(), [](NodeLocalTransformBuffer& v) { v.localAnimated_ = glm::mat3x4(1.f);  });
		size_t size = vector_sizeof(nt);

		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
// 			.setQueueFamilyIndexCount(1)
// 			.setPQueueFamilyIndices(&device.getQueueFamilyIndex())
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::NODE_LOCAL_TRANSFORM);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, nt.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
	}


	//	NodeGlobalTransformBuffer
	{
		std::vector<NodeGlobalTransformBuffer> nt(mPrivate->mNodeRoot.mNodeList.size() * instanceNum);
		std::for_each(nt.begin(), nt.end(), [](NodeGlobalTransformBuffer& v) { v.globalAnimated_ = glm::mat4(1.f);  });
		size_t size = vector_sizeof(nt);

		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
// 			.setQueueFamilyIndexCount(familyIndex.size())
// 			.setPQueueFamilyIndices(familyIndex.data())
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::NODE_GLOBAL_TRANSFORM);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, nt.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
	}
	// world
	{
		std::vector<glm::mat4> world(instanceNum);
		std::for_each(world.begin(), world.end(),
			[](decltype(world)::value_type& m)
		{
			m = glm::transpose(glm::mat4x3(glm::translate(glm::mat4(1.f), glm::ballRand(3000.f))));
					m = glm::translate(glm::mat4(1.f), glm::ballRand(3000.f));
		}
		);
		size_t size = vector_sizeof(world);

		vk::BufferCreateInfo bufferInfo = vk::BufferCreateInfo()
			.setUsage(vk::BufferUsageFlagBits::eUniformBuffer)
// 			.setQueueFamilyIndexCount(1)
// 			.setPQueueFamilyIndices(&device.getQueueFamilyIndex())
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);
		auto& buffer = mPrivate->getBuffer(Private::ModelBuffer::WORLD);
		buffer.mBuffer = device->createBuffer(bufferInfo);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(buffer.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		buffer.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(buffer.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, size, world.data(), size);
		}
		device->unmapMemory(buffer.mMemory);
		device->bindBufferMemory(buffer.mBuffer, buffer.mMemory, 0);

		buffer.mBufferInfo.setBuffer(buffer.mBuffer);
		buffer.mBufferInfo.setOffset(0);
		buffer.mBufferInfo.setRange(size);
	}


	mPrivate->mVertexNum = std::move(vertexSize);
	mPrivate->mIndexNum = std::move(indexSize);

//	m_cmd[0].end();


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
		auto cmd_pool = sThreadData::Order().getCmdPoolCompiled(device.getQueueFamilyIndex())[0];
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

// void cModel::execute()
// {
// 	{
// 		auto b = mPrivate->getBuffer(Private::ModelBuffer::BONE_INFO);
// 		vk::MemoryRequirements memoryRequest = mPrivate->mDevice->getBufferMemoryRequirements(b.mBuffer);
// 		auto* mem = reinterpret_cast<BoneInfo*>(mPrivate->mDevice->mapMemory(b.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
// 		//		printf("%d\n", mem->mInstanceNum);
// 		mPrivate->mDevice->unmapMemory(b.mMemory);
// 	}
// 	{
// 		auto b = mPrivate->getBuffer(Private::ModelBuffer::NODE_GLOBAL_TRANSFORM);
// 		vk::MemoryRequirements memoryRequest = mPrivate->mDevice->getBufferMemoryRequirements(b.mBuffer);
// 		auto* mem = reinterpret_cast<NodeGlobalTransformBuffer*>(mPrivate->mDevice->mapMemory(b.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
// 		//		printf("%d\n", mem->mInstanceNum);
// 		mPrivate->mDevice->unmapMemory(b.mMemory);
// 	}
// 	{
// 		auto b = mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM);
// 		vk::MemoryRequirements memoryRequest = mPrivate->mDevice->getBufferMemoryRequirements(b.mBuffer);
// 		auto* mem = reinterpret_cast<BoneTransformBuffer*>(mPrivate->mDevice->mapMemory(b.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
// 		//		printf("%d\n", mem->mInstanceNum);
// 		mPrivate->mDevice->unmapMemory(b.mMemory);
// 	}
//
//}
