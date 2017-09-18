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

ResourceManager<ResourceTexture::Resource> ResourceTexture::s_manager;
ResourceManager<cModel::Resource> cModel::s_manager;
void ResourceTexture::load(std::shared_ptr<btr::Loader>& loader, vk::CommandBuffer cmd, const std::string& filename)
{
	if (s_manager.manage(m_resource, filename)) {
		return;
	}
	m_resource->m_device = loader->m_device;

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
	vk::UniqueImage image = loader->m_device->createImageUnique(image_info);

	vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(image.get());
	vk::MemoryAllocateInfo memory_alloc_info;
	memory_alloc_info.allocationSize = memory_request.size;
	memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::UniqueDeviceMemory image_memory = loader->m_device->allocateMemoryUnique(memory_alloc_info);
	loader->m_device->bindImageMemory(image.get(), image_memory.get(), 0);

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


		{
			vk::ImageMemoryBarrier to_copy_barrier;
			to_copy_barrier.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_copy_barrier.image = image.get();
			to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
			to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_copy_barrier.subresourceRange = subresourceRange;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
		}
		cmd.copyBufferToImage(staging_buffer.getBufferInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
		{
			vk::ImageMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.image = image.get();
			to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			to_shader_read_barrier.subresourceRange = subresourceRange;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
		}

	}

	vk::ImageViewCreateInfo view_info;
	view_info.viewType = vk::ImageViewType::e2D;
	view_info.components.r = vk::ComponentSwizzle::eR;
	view_info.components.g = vk::ComponentSwizzle::eG;
	view_info.components.b = vk::ComponentSwizzle::eB;
	view_info.components.a = vk::ComponentSwizzle::eA;
	view_info.flags = vk::ImageViewCreateFlags();
	view_info.format = vk::Format::eR32G32B32A32Sfloat;
	view_info.image = image.get();
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

	m_resource->m_image = std::move(image) ;
	m_resource->m_memory = std::move(image_memory);
	m_resource->m_image_view = loader->m_device->createImageViewUnique(view_info);
	m_resource->m_sampler = loader->m_device->createSamplerUnique(sampler_info);
}


std::vector<cModel::Material> loadMaterial(const aiScene* scene, const std::string& filename, std::shared_ptr<btr::Loader>& loader, vk::CommandBuffer cmd)
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
			mat.mDiffuseTex.load(loader, cmd, path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mAmbientTex.load(loader, cmd, path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mSpecularTex.load(loader, cmd, path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mNormalTex.load(loader, cmd, path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mHeightTex.load(loader, cmd, path + "/" + str.C_Str());
		}
	}
	return material;
}

void _loadNodeRecurcive(aiNode* ainode, RootNode& root, int parent, uint32_t depth)
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
	root.m_depth_max = glm::max(root.m_depth_max, depth);
	for (s32 i = 0; i < (int)ainode->mNumChildren; i++) {
		_loadNodeRecurcive(ainode->mChildren[i], root, nodeIndex, depth+1);
	}
}
RootNode loadNode(const aiScene* scene)
{
	RootNode root;
	root.mNodeList.clear();
	root.mNodeList.reserve(countAiNode(scene->mRootNode));
	_loadNodeRecurcive(scene->mRootNode, root, -1, 0);
	return root;
}

void loadMotion(cAnimation& anim_buffer, const aiScene* scene, const RootNode& root, std::shared_ptr<btr::Loader>& loader)
{
	if (!scene->HasAnimations()) {
		return;
	}
	anim_buffer.m_motion.resize(scene->mNumAnimations);
	for (size_t i = 0; i < scene->mNumAnimations; i++)
	{
		aiAnimation* anim = scene->mAnimations[i];
		auto& motion = anim_buffer.m_motion[i];
		motion = std::make_shared<cMotion>();
		motion->m_name = anim->mName.C_Str();
		motion->m_duration = (float)anim->mDuration;
		motion->m_ticks_per_second = anim->mTicksPerSecond == 0.f ? 60.f : (float)anim->mTicksPerSecond;
		motion->m_data.resize(anim->mNumChannels);
		for (size_t channel_index = 0; channel_index < anim->mNumChannels; channel_index++)
		{
			aiNodeAnim* aiAnim = anim->mChannels[channel_index];
			auto no = root.getNodeIndexByName(aiAnim->mNodeName.C_Str());
			auto& node_motion = motion->m_data[no];
			node_motion.m_nodename = aiAnim->mNodeName.C_Str();
			node_motion.m_node_index = no;
			node_motion.m_translate.resize(aiAnim->mNumPositionKeys);
			for (uint32_t pos_index = 0; pos_index < aiAnim->mNumPositionKeys; pos_index++)
			{
				node_motion.m_translate[pos_index].m_time = (float)aiAnim->mPositionKeys[pos_index].mTime;
				node_motion.m_translate[pos_index].m_data.x = aiAnim->mPositionKeys[pos_index].mValue.x;
				node_motion.m_translate[pos_index].m_data.y = aiAnim->mPositionKeys[pos_index].mValue.y;
				node_motion.m_translate[pos_index].m_data.z = aiAnim->mPositionKeys[pos_index].mValue.z;
			}
			node_motion.m_scale.resize(aiAnim->mNumScalingKeys);
			for (uint32_t scale_index = 0; scale_index < aiAnim->mNumScalingKeys; scale_index++)
			{
				node_motion.m_scale[scale_index].m_time = (float)aiAnim->mScalingKeys[scale_index].mTime;
				node_motion.m_scale[scale_index].m_data.x = aiAnim->mScalingKeys[scale_index].mValue.x;
				node_motion.m_scale[scale_index].m_data.y = aiAnim->mScalingKeys[scale_index].mValue.y;
				node_motion.m_scale[scale_index].m_data.z = aiAnim->mScalingKeys[scale_index].mValue.z;
			}
			node_motion.m_rotate.resize(aiAnim->mNumRotationKeys);
			for (uint32_t rot_index = 0; rot_index < aiAnim->mNumRotationKeys; rot_index++)
			{
				node_motion.m_rotate[rot_index].m_time = (float)aiAnim->mRotationKeys[rot_index].mTime;
				node_motion.m_rotate[rot_index].m_data.x = aiAnim->mRotationKeys[rot_index].mValue.x;
				node_motion.m_rotate[rot_index].m_data.y = aiAnim->mRotationKeys[rot_index].mValue.y;
				node_motion.m_rotate[rot_index].m_data.z = aiAnim->mRotationKeys[rot_index].mValue.z;
				node_motion.m_rotate[rot_index].m_data.w = aiAnim->mRotationKeys[rot_index].mValue.w;
			}
		}

	}


}


cModel::cModel()
{
}
cModel::~cModel()
{}


void cModel::load(std::shared_ptr<btr::Loader>& loader, const std::string& filename)
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


	auto& device = sGlobal::Order().getGPU(0).getDevice();

	auto cmd = loader->m_cmd_pool->allocCmdTempolary(0);

	// 初期化
	m_resource->m_material = loadMaterial(scene, filename, loader, cmd.get());
	m_resource->mNodeRoot = loadNode(scene);
	loadMotion(m_resource->m_animation, scene, m_resource->mNodeRoot, loader);

	std::vector<Bone>& boneList = m_resource->mBone;

	unsigned numIndex = 0;
	unsigned numVertex = 0;
	m_resource->m_mesh.resize(scene->mNumMeshes);
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		m_resource->m_mesh[i].m_draw_cmd.indexCount = scene->mMeshes[i]->mNumFaces * 3;
		m_resource->m_mesh[i].m_draw_cmd.firstIndex = numIndex;
		m_resource->m_mesh[i].m_draw_cmd.instanceCount = 100;
		m_resource->m_mesh[i].m_draw_cmd.vertexOffset = 0;
		m_resource->m_mesh[i].m_draw_cmd.firstInstance = 0;
		m_resource->m_mesh[i].m_vertex_num = scene->mMeshes[i]->mNumVertices;
		m_resource->m_mesh[i].m_material_index = scene->mMeshes[i]->mMaterialIndex;

		numVertex += scene->mMeshes[i]->mNumVertices;
		numIndex += scene->mMeshes[i]->mNumFaces * 3;
	}

	btr::BufferMemory::Descriptor staging_vertex_desc;
	staging_vertex_desc.size = sizeof(Vertex) * numVertex;
	staging_vertex_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_vertex = loader->m_staging_memory.allocateMemory(staging_vertex_desc);

	auto index_type = numVertex < std::numeric_limits<uint16_t>::max() ? vk::IndexType::eUint16 : vk::IndexType::eUint32;

	btr::BufferMemory::Descriptor staging_index_desc;
	staging_index_desc.size = (index_type == vk::IndexType::eUint16 ? sizeof(uint16_t) : sizeof(uint32_t)) * numIndex;
	staging_index_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_index = loader->m_staging_memory.allocateMemory(staging_index_desc);
	auto index_stride = (index_type == vk::IndexType::eUint16 ? sizeof(uint16_t) : sizeof(uint32_t));
	auto* index = static_cast<char*>(staging_index.getMappedPtr());
	Vertex* vertex = static_cast<Vertex*>(staging_vertex.getMappedPtr());
	memset(vertex, -1, staging_vertex.getBufferInfo().range);
	uint32_t v_count = 0;
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[i];
 
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
		for (uint32_t v = 0; v < mesh->mNumVertices; v++){
			_vertex[v].m_position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
		}
		glm::vec3 max(-10e10f);
		glm::vec3 min(10e10f);
		for (unsigned i = 0; i < mesh->mNumVertices; i++)
		{
			auto& v = _vertex[i];
			max.x = glm::max(max.x, v.m_position.x);
			max.y = glm::max(max.y, v.m_position.y);
			max.z = glm::max(max.z, v.m_position.z);
			min.x = glm::min(min.x, v.m_position.x);
			min.y = glm::min(min.y, v.m_position.y);
			min.z = glm::min(min.z, v.m_position.z);
		}
		m_resource->m_mesh[i].mAABB = glm::vec4((max - min).xyz, glm::length((max - min)));

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
					assert(m_resource->mNodeRoot.getNodeByIndex(bone.mNodeIndex)->m_bone_index == -1);
					m_resource->mNodeRoot.getNodeByIndex(bone.mNodeIndex)->m_bone_index = index;
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

	{
		cMeshResource& mesh = m_resource->m_mesh_resource;

		{
			mesh.m_vertex_buffer_ex = loader->m_vertex_memory.allocateMemory(staging_vertex.getBufferInfo().range);
			mesh.m_index_buffer_ex = loader->m_vertex_memory.allocateMemory(staging_index.getBufferInfo().range);

			btr::BufferMemory::Descriptor indirect_desc;
			indirect_desc.size = vector_sizeof(m_resource->m_mesh);
			mesh.m_indirect_buffer_ex = loader->m_vertex_memory.allocateMemory(indirect_desc);

			indirect_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_indirect = loader->m_staging_memory.allocateMemory(indirect_desc);
			auto* indirect = staging_indirect.getMappedPtr<Mesh>(0);
			int offset = 0;
			for (size_t i = 0; i < m_resource->m_mesh.size(); i++) {
				indirect[i].m_draw_cmd.indexCount = m_resource->m_mesh[i].m_draw_cmd.indexCount;
				indirect[i].m_draw_cmd.firstIndex = offset;
				indirect[i].m_draw_cmd.instanceCount = 100;
				indirect[i].m_draw_cmd.vertexOffset = 0;
				indirect[i].m_draw_cmd.firstInstance = 0;
				offset += indirect[i].m_draw_cmd.indexCount;
			}
			vk::BufferCopy copy_info;
			copy_info.setSize(staging_vertex.getBufferInfo().range);
			copy_info.setSrcOffset(staging_vertex.getBufferInfo().offset);
			copy_info.setDstOffset(mesh.m_vertex_buffer_ex.getBufferInfo().offset);
			cmd->copyBuffer(staging_vertex.getBufferInfo().buffer, mesh.m_vertex_buffer_ex.getBufferInfo().buffer, copy_info);

			copy_info.setSize(staging_index.getBufferInfo().range);
			copy_info.setSrcOffset(staging_index.getBufferInfo().offset);
			copy_info.setDstOffset(mesh.m_index_buffer_ex.getBufferInfo().offset);
			cmd->copyBuffer(staging_index.getBufferInfo().buffer, mesh.m_index_buffer_ex.getBufferInfo().buffer, copy_info);

			copy_info.setSize(staging_indirect.getBufferInfo().range);
			copy_info.setSrcOffset(staging_indirect.getBufferInfo().offset);
			copy_info.setDstOffset(mesh.m_indirect_buffer_ex.getBufferInfo().offset);
			cmd->copyBuffer(staging_indirect.getBufferInfo().buffer, mesh.m_indirect_buffer_ex.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier indirect_barrier;
			indirect_barrier.setBuffer(mesh.m_indirect_buffer_ex.getBufferInfo().buffer);
			indirect_barrier.setOffset(mesh.m_indirect_buffer_ex.getBufferInfo().offset);
			indirect_barrier.setSize(mesh.m_indirect_buffer_ex.getBufferInfo().range);
			indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd->pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eDrawIndirect,
				vk::DependencyFlags(),
				{}, { indirect_barrier }, {});

			mesh.mIndexType = index_type;
			mesh.mIndirectCount = (int32_t)m_resource->m_mesh.size();
		}
	}

	m_resource->m_model_info.mNodeNum = (s32)m_resource->mNodeRoot.mNodeList.size();
	m_resource->m_model_info.mBoneNum = (s32)m_resource->mBone.size();
	m_resource->m_model_info.mMeshNum = (s32)m_resource->m_mesh.size();
	m_resource->m_model_info.m_node_depth_max = m_resource->mNodeRoot.m_depth_max;
	// todo
	glm::vec3 max(-10e10f);
	glm::vec3 min(10e10f);
	m_resource->m_model_info.mAabb = glm::vec4((max - min).xyz, glm::length((max - min)));

	m_resource->m_model_info.mInvGlobalMatrix = glm::inverse(m_resource->mNodeRoot.getRootNode()->mTransformation);


	auto e = std::chrono::system_clock::now();
	auto t = std::chrono::duration_cast<std::chrono::microseconds>(e - s);
	sDebug::Order().print(sDebug::FLAG_LOG | sDebug::SOURCE_MODEL, "[Load Complete %6.2fs] %s NodeNum = %d BoneNum = %d \n", t.count() / 1000.f / 1000.f, filename.c_str(), m_resource->mNodeRoot.mNodeList.size(), m_resource->mBone.size());

}


std::string cModel::getFilename() const
{
	return m_resource ? m_resource->m_filename : "";
}
const cMeshResource* cModel::getMesh() const
{
	return m_resource ? &m_resource->m_mesh_resource : nullptr;
}
