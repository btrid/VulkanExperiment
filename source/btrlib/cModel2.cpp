#include <btrlib/Define.h>
#include <chrono>
#include <filesystem>
#include <algorithm>

#include <btrlib/cModel2.h>
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

std::vector<cModel2::Material> loadMaterial(const aiScene* scene, const std::string& filename, const std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd)
{
	std::string path = std::experimental::filesystem::path(filename).remove_filename().string();
	std::vector<cModel2::Material> material(scene->mNumMaterials);
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
			mat.mDiffuseTex.load(context, cmd, path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mAmbientTex.load(context, cmd, path + "/" + str.C_Str());
		}
		if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mSpecularTex.load(context, cmd, path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mNormalTex.load(context, cmd, path + "/" + str.C_Str());
		}

		if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
			mat.mHeightTex.load(context, cmd, path + "/" + str.C_Str());
		}
	}
	return material;
}

void _loadNodeRecurcive(RootNode& root, aiNode* ainode, int parent, uint32_t depth)
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
		_loadNodeRecurcive(root, ainode->mChildren[i], nodeIndex, depth + 1);
	}
}
RootNode loadNode(const aiScene* scene)
{
	RootNode root;
	root.mNodeList.clear();
	root.mNodeList.reserve(countAiNode(scene->mRootNode));
	_loadNodeRecurcive(root, scene->mRootNode, -1, 0);
	return root;
}

void loadMotion(cAnimation& anim_buffer, const aiScene* scene, const RootNode& root, const std::shared_ptr<btr::Context>& context)
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
		motion->m_node_num = (uint32_t)root.mNodeList.size();
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

std::shared_ptr<cModel2> load(const std::shared_ptr<btr::Context>& context, const std::string& filename)
{
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

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filename, OREORE_PRESET);
	if (!scene) {
		sDebug::Order().print(sDebug::FLAG_ERROR | sDebug::ACTION_ASSERTION, "can't file load in cModel2::load : %s : %s\n", filename.c_str(), importer.GetErrorString());
		return;
	}


	auto resource = std::make_shared<cModel2>();
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	// 初期化
	resource->mMaterial = loadMaterial(scene, filename, context, cmd);
	resource->mNodeRoot = loadNode(scene);
	loadMotion(resource->m_animation, scene, resource->mNodeRoot, context);

	std::vector<cModel2::Bone>& boneList = resource->mBone;

	unsigned numIndex = 0;
	unsigned numVertex = 0;
	resource->m_mesh.resize(scene->mNumMeshes);
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		resource->m_mesh[i].m_draw_cmd.indexCount = scene->mMeshes[i]->mNumFaces * 3;
		resource->m_mesh[i].m_draw_cmd.firstIndex = numIndex;
		resource->m_mesh[i].m_draw_cmd.instanceCount = 1;
		resource->m_mesh[i].m_draw_cmd.vertexOffset = 0;
		resource->m_mesh[i].m_draw_cmd.firstInstance = 0;
		resource->m_mesh[i].m_vertex_num = scene->mMeshes[i]->mNumVertices;
		resource->m_mesh[i].m_material_index = scene->mMeshes[i]->mMaterialIndex;

		numVertex += scene->mMeshes[i]->mNumVertices;
		numIndex += scene->mMeshes[i]->mNumFaces * 3;
	}

	btr::BufferMemoryDescriptor staging_vertex_desc;
	staging_vertex_desc.size = sizeof(Vertex) * numVertex;
	staging_vertex_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_vertex = context->m_staging_memory.allocateMemory(staging_vertex_desc);

	auto index_type = numVertex < std::numeric_limits<uint16_t>::max() ? vk::IndexType::eUint16 : vk::IndexType::eUint32;

	btr::BufferMemoryDescriptor staging_index_desc;
	staging_index_desc.size = (index_type == vk::IndexType::eUint16 ? sizeof(uint16_t) : sizeof(uint32_t)) * numIndex;
	staging_index_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_index = context->m_staging_memory.allocateMemory(staging_index_desc);
	auto index_stride = (index_type == vk::IndexType::eUint16 ? sizeof(uint16_t) : sizeof(uint32_t));
	auto* index = static_cast<char*>(staging_index.getMappedPtr());
	Vertex* vertex = static_cast<Vertex*>(staging_vertex.getMappedPtr());
	memset(vertex, -1, staging_vertex.getInfo().range);
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
		for (uint32_t v = 0; v < mesh->mNumVertices; v++) {
			_vertex[v].m_position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
		}
		glm::vec3 max(-10e10f);
		glm::vec3 min(10e10f);
		for (unsigned i = 0; i < mesh->mNumVertices; i++)
		{
			auto& v = _vertex[i];
			max.x = glm::max(max, v.m_position);
			max.y = glm::max(max.y, v.m_position.y);
			max.z = glm::max(max.z, v.m_position.z);
			min.x = glm::min(min.x, v.m_position.x);
			min.y = glm::min(min.y, v.m_position.y);
			min.z = glm::min(min.z, v.m_position.z);
		}
		resource->m_mesh[i].mAABB = glm::vec4((max - min).xyz, glm::length((max - min)));

		if (mesh->HasNormals()) 
		{
			for (size_t v = 0; v < mesh->mNumVertices; v++) 
			{
				_vertex[v].m_normal = glm::packF2x11_1x10(vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z));
			}
		}
		if (mesh->HasTextureCoords(0)) 
		{
			for (size_t v = 0; v < mesh->mNumVertices; v++) 
			{
				_vertex[v].m_texcoord0 = glm::packSnorm3x10_1x2(vec4(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y, mesh->mTextureCoords[0][v].z, 0.f));
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
				if (index == 0xffui8) 
				{
					// 新しいボーンの登録
					cModel2::Bone bone;
					bone.mName = mesh->mBones[b]->mName.C_Str();
					bone.mOffset = AI_TO(mesh->mBones[b]->mOffsetMatrix);
					bone.mNodeIndex = resource->mNodeRoot.getNodeIndexByName(mesh->mBones[b]->mName.C_Str());
					boneList.emplace_back(bone);
					index = (int)boneList.size() - 1;
					assert(index < 0xffui8);
					assert(resource->mNodeRoot.mNodeList[bone.mNodeIndex].m_bone_index == -1);
					resource->mNodeRoot.mNodeList[bone.mNodeIndex].m_bone_index = index;
				}

				for (size_t i = 0; i < mesh->mBones[b]->mNumWeights; i++)
				{
					aiVertexWeight& weight = mesh->mBones[b]->mWeights[i];
					Vertex& v = _vertex[weight.mVertexId];
					for (glm::length_t o = 0; o < cModel2::BONE_BLEND_MAX; o++) {
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
		ResourceVertex& vertex_data = resource->m_mesh_resource;

		{
			vertex_data.m_vertex_buffer = context->m_vertex_memory.allocateMemory(staging_vertex.getInfo().range);
			vertex_data.m_index_buffer = context->m_vertex_memory.allocateMemory(staging_index.getInfo().range);

			btr::BufferMemoryDescriptor indirect_desc;
			indirect_desc.size = vector_sizeof(resource->m_mesh);
			vertex_data.m_indirect_buffer = context->m_vertex_memory.allocateMemory(indirect_desc);

			indirect_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_indirect = context->m_staging_memory.allocateMemory(indirect_desc);
			auto* indirect = staging_indirect.getMappedPtr<cModel2::Mesh>(0);
			memcpy_s(indirect, indirect_desc.size, resource->m_mesh.data(), indirect_desc.size);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_vertex.getInfo().range);
			copy_info.setSrcOffset(staging_vertex.getInfo().offset);
			copy_info.setDstOffset(vertex_data.m_vertex_buffer.getInfo().offset);
			cmd.copyBuffer(staging_vertex.getInfo().buffer, vertex_data.m_vertex_buffer.getInfo().buffer, copy_info);

			copy_info.setSize(staging_index.getInfo().range);
			copy_info.setSrcOffset(staging_index.getInfo().offset);
			copy_info.setDstOffset(vertex_data.m_index_buffer.getInfo().offset);
			cmd.copyBuffer(staging_index.getInfo().buffer, vertex_data.m_index_buffer.getInfo().buffer, copy_info);

			copy_info.setSize(staging_indirect.getInfo().range);
			copy_info.setSrcOffset(staging_indirect.getInfo().offset);
			copy_info.setDstOffset(vertex_data.m_indirect_buffer.getInfo().offset);
			cmd.copyBuffer(staging_indirect.getInfo().buffer, vertex_data.m_indirect_buffer.getInfo().buffer, copy_info);

			vk::BufferMemoryBarrier indirect_barrier;
			indirect_barrier.setBuffer(vertex_data.m_indirect_buffer.getInfo().buffer);
			indirect_barrier.setOffset(vertex_data.m_indirect_buffer.getInfo().offset);
			indirect_barrier.setSize(vertex_data.m_indirect_buffer.getInfo().range);
			indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eDrawIndirect,
				vk::DependencyFlags(),
				{}, { indirect_barrier }, {});

			vertex_data.mIndexType = index_type;
			vertex_data.mIndirectCount = (int32_t)resource->m_mesh.size();

// 			vertex_data.m_vertex_input_binding = GetVertexInputBinding();
// 			vertex_data.m_vertex_input_attribute = GetVertexInputAttribute();
// 			vertex_data.m_vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_data.m_vertex_input_binding.size());
// 			vertex_data.m_vertex_input_info.setPVertexBindingDescriptions(vertex_data.m_vertex_input_binding.data());
// 			vertex_data.m_vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_data.m_vertex_input_attribute.size());
// 			vertex_data.m_vertex_input_info.setPVertexAttributeDescriptions(vertex_data.m_vertex_input_attribute.data());
		}
	}

	resource->m_model_info.mNodeNum = (s32)resource->mNodeRoot.mNodeList.size();
	resource->m_model_info.mBoneNum = (s32)resource->mBone.size();
	resource->m_model_info.mMeshNum = (s32)resource->m_mesh.size();
	resource->m_model_info.m_node_depth_max = resource->mNodeRoot.m_depth_max;
	// todo
	glm::vec3 max(-10e10f);
	glm::vec3 min(10e10f);
	resource->m_model_info.mAabb = glm::vec4((max - min).xyz, glm::length((max - min)));

	resource->m_model_info.mInvGlobalMatrix = glm::inverse(resource->mNodeRoot.mNodeList[0].mTransformation);
}

