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
#include <assimp/pbrmaterial.h>

namespace {
	glm::mat4 AI_TO(aiMatrix4x4& from)
	{
		mat4 to;
		memcpy(&to, &from, sizeof(to));
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

ResourceManager<cModel::Resource> cModel::s_manager;

std::vector<cModel::Material> loadMaterial(const aiScene* scene, const std::string& filename, const std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd)
{
	std::string path = std::filesystem::path(filename).remove_filename().string();
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
		struct  
		{
			aiTextureType type;
			cModel::E index;
		}tex_type[] = 
		{
			{aiTextureType_DIFFUSE, cModel::ResourceTextureIndex_Diffuse},
			{aiTextureType_AMBIENT, cModel::ResourceTextureIndex_Ambient},
			{aiTextureType_SPECULAR, cModel::ResourceTextureIndex_Specular},
			{aiTextureType_NORMALS, cModel::ResourceTextureIndex_Normal},
			{aiTextureType_HEIGHT, cModel::ResourceTextureIndex_Height},
			{aiTextureType_BASE_COLOR, cModel::ResourceTextureIndex_Base},
			{aiTextureType_NORMAL_CAMERA, cModel::ResourceTextureIndex_NormalCamera},
 			{aiTextureType_EMISSION_COLOR, cModel::ResourceTextureIndex_Emissive},
			{aiTextureType_METALNESS, cModel::ResourceTextureIndex_Metalness},
			{aiTextureType_DIFFUSE_ROUGHNESS, cModel::ResourceTextureIndex_DiffuseRoughness},
			{aiTextureType_AMBIENT_OCCLUSION, cModel::ResourceTextureIndex_AmbientOcclusion},
 		};
		for (auto& t : tex_type)
		{
			if (aiMat->GetTexture(t.type, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == aiReturn_SUCCESS) { mat.mTex[t.index].load(context, cmd, path + "/" + str.C_Str()); }
		}
		if (aiMat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == aiReturn_SUCCESS) { mat.mTex[cModel::ResourceTextureIndex_Diffuse].load(context, cmd, path + "/" + str.C_Str()); };
		if (aiMat->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == aiReturn_SUCCESS) { mat.mTex[cModel::ResourceTextureIndex_Metalness].load(context, cmd, path + "/" + str.C_Str()); };
		//		AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE
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
	root.m_depth_max = 0;
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
void cModel::load(const std::shared_ptr<btr::Context>& context, const std::string& filename)
{

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


	auto& device = context->m_device;
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	// 初期化
	m_resource->m_material = loadMaterial(scene, filename, context, cmd);
	m_resource->mNodeRoot = loadNode(scene);
	loadMotion(m_resource->m_animation, scene, m_resource->mNodeRoot, context);

	std::vector<Bone>& boneList = m_resource->mBone;

	unsigned numIndex = 0;
	unsigned numVertex = 0;
	m_resource->m_mesh.resize(scene->mNumMeshes);
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		m_resource->m_mesh[i].m_draw_cmd.indexCount = scene->mMeshes[i]->mNumFaces * 3;
		m_resource->m_mesh[i].m_draw_cmd.firstIndex = numIndex * 3;
		m_resource->m_mesh[i].m_draw_cmd.instanceCount = 1;
		m_resource->m_mesh[i].m_draw_cmd.vertexOffset = 0;
		m_resource->m_mesh[i].m_draw_cmd.firstInstance = 0;
		m_resource->m_mesh[i].m_vertex_num = scene->mMeshes[i]->mNumVertices;
		m_resource->m_mesh[i].m_material_index = scene->mMeshes[i]->mMaterialIndex;

		numVertex += scene->mMeshes[i]->mNumVertices;
		numIndex += scene->mMeshes[i]->mNumFaces;
	}


	auto vertex = context->m_staging_memory.allocateMemory<aiVector3D>(numVertex, true);
	auto normal = context->m_staging_memory.allocateMemory<uint32_t>(numVertex, true);
	auto texcoord = context->m_staging_memory.allocateMemory<aiVector3D>(numVertex, true);
	auto index = context->m_staging_memory.allocateMemory<uvec3>(numIndex, true);

	auto bone_id = context->m_staging_memory.allocateMemory<u8vec4>(numVertex, true);
	std::fill(bone_id.getMappedPtr(), bone_id.getMappedPtr()+ numVertex, u8vec4(-1ui8));
	auto bone_weight = context->m_staging_memory.allocateMemory<u8vec4>(numVertex, true);

	uint32_t index_offset = 0;
	uint32_t vertex_offset = 0;

	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[i];
		vec3 max(-10e10f);
		vec3 min(10e10f);
		for (u32 n = 0; n < mesh->mNumFaces; n++)
		{
			(*index.getMappedPtr(index_offset)) = uvec3(mesh->mFaces[n].mIndices[0], mesh->mFaces[n].mIndices[1], mesh->mFaces[n].mIndices[2]) + vertex_offset;
			index_offset++;
		}
		for (unsigned i = 0; i < mesh->mNumVertices; i++)
		{
			auto& v = mesh->mVertices[i];
			max.x = glm::max(max.x, v.x);
			max.y = glm::max(max.y, v.y);
			max.z = glm::max(max.z, v.z);
			min.x = glm::min(min.x, v.x);
			min.y = glm::min(min.y, v.y);
			min.z = glm::min(min.z, v.z);
		}
		m_resource->m_mesh[i].mAABB = glm::vec4((max - min).xyz, glm::length((max - min)));

		std::copy(mesh->mVertices, mesh->mVertices + mesh->mNumVertices, vertex.getMappedPtr(vertex_offset));
		for (unsigned v = 0; v < mesh->mNumVertices; v++) { *normal.getMappedPtr(vertex_offset+v) = glm::packSnorm3x10_1x2(vec4(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z, 1.f)); }
		std::copy(mesh->mTextureCoords[0], mesh->mTextureCoords[0] + mesh->mNumVertices, texcoord.getMappedPtr(vertex_offset));


		// SkinMesh
		if (mesh->HasBones())
		{
			for (size_t b = 0; b < mesh->mNumBones; b++)
			{
				// BoneがMeshからしか参照できないので全部展開する
				u8 index = 0xffui8;
				for (size_t k = 0; k < boneList.size(); k++)
				{
					if (boneList[k].mName.compare(mesh->mBones[b]->mName.C_Str()) == 0)
					{
						index = (int)k;
						break;
					}

				}
				if (index == 0xffui8)
				{
					// 新しいボーンの登録
					Bone bone;
					bone.mName = mesh->mBones[b]->mName.C_Str();
					bone.mOffset = AI_TO(mesh->mBones[b]->mOffsetMatrix);
					bone.mNodeIndex = m_resource->mNodeRoot.getNodeIndexByName(mesh->mBones[b]->mName.C_Str());
					boneList.emplace_back(bone);
					index = (int)boneList.size() - 1;
					assert(index < 0xffui8);
					assert(m_resource->mNodeRoot.mNodeList[bone.mNodeIndex].m_bone_index == -1);
					m_resource->mNodeRoot.mNodeList[bone.mNodeIndex].m_bone_index = index;
				}

				for (size_t i = 0; i < mesh->mBones[b]->mNumWeights; i++)
				{
					aiVertexWeight& weight = mesh->mBones[b]->mWeights[i];
					for (glm::length_t o = 0; o < BONE_NUM; o++) 
					{
						if ((*bone_id.getMappedPtr(vertex_offset+weight.mVertexId))[o] == 0xffui8)
						{
							(*bone_id.getMappedPtr(vertex_offset + weight.mVertexId))[o] = index;
							(*bone_weight.getMappedPtr(vertex_offset + weight.mVertexId))[o] = glm::packUnorm1x8(weight.mWeight);
							break;
						}
					}
				}
			}
		}

		vertex_offset += mesh->mNumVertices;
	}
	importer.FreeScene();

	{
		VertexResource& vertex_data = m_resource->m_vertex;

		{
			vertex_data.v_position = context->m_vertex_memory.allocateMemory<vec3>(numVertex);
			vertex_data.v_normal = context->m_vertex_memory.allocateMemory<uint32_t>(numVertex);
			vertex_data.v_texcoord = context->m_vertex_memory.allocateMemory<vec3>(numVertex);
			vertex_data.v_bone_id = context->m_vertex_memory.allocateMemory<u8vec4>(numVertex);
			vertex_data.v_bone_weight = context->m_vertex_memory.allocateMemory<u8vec4>(numVertex);
			vertex_data.v_index = context->m_vertex_memory.allocateMemory<uvec3>(numIndex);

			vk::BufferCopy copy_info;
			copy_info.setSize(vertex.getInfo().range).setSrcOffset(vertex.getInfo().offset).setDstOffset(vertex_data.v_position.getInfo().offset);
			cmd.copyBuffer(vertex.getInfo().buffer, vertex_data.v_position.getInfo().buffer, copy_info);

			copy_info.setSize(normal.getInfo().range).setSrcOffset(normal.getInfo().offset).setDstOffset(vertex_data.v_normal.getInfo().offset);
			cmd.copyBuffer(normal.getInfo().buffer, vertex_data.v_normal.getInfo().buffer, copy_info);

			copy_info.setSize(texcoord.getInfo().range).setSrcOffset(texcoord.getInfo().offset).setDstOffset(vertex_data.v_texcoord.getInfo().offset);
			cmd.copyBuffer(texcoord.getInfo().buffer, vertex_data.v_texcoord.getInfo().buffer, copy_info);


			copy_info.setSize(bone_id.getInfo().range).setSrcOffset(bone_id.getInfo().offset).setDstOffset(vertex_data.v_bone_id.getInfo().offset);
			cmd.copyBuffer(bone_id.getInfo().buffer, vertex_data.v_bone_id.getInfo().buffer, copy_info);

			copy_info.setSize(bone_weight.getInfo().range).setSrcOffset(bone_weight.getInfo().offset).setDstOffset(vertex_data.v_bone_weight.getInfo().offset);
			cmd.copyBuffer(bone_weight.getInfo().buffer, vertex_data.v_bone_weight.getInfo().buffer, copy_info);

			copy_info.setSize(index.getInfo().range).setSrcOffset(index.getInfo().offset).setDstOffset(vertex_data.v_index.getInfo().offset);
			cmd.copyBuffer(index.getInfo().buffer, vertex_data.v_index.getInfo().buffer, copy_info);


			vertex_data.v_indirect = context->m_vertex_memory.allocateMemory<Mesh>(m_resource->m_mesh.size());
			auto indirect = context->m_staging_memory.allocateMemory<Mesh>(m_resource->m_mesh.size(), true);
			memcpy_s(indirect.getMappedPtr(), vector_sizeof(m_resource->m_mesh), m_resource->m_mesh.data(), vector_sizeof(m_resource->m_mesh));

			copy_info.setSize(indirect.getInfo().range).setSrcOffset(indirect.getInfo().offset).setDstOffset(vertex_data.v_indirect.getInfo().offset);
			cmd.copyBuffer(indirect.getInfo().buffer, vertex_data.v_indirect.getInfo().buffer, copy_info);

			vk::BufferMemoryBarrier indirect_barrier = vertex_data.v_indirect.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier( vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { indirect_barrier }, {});

			vertex_data.mIndexType = vk::IndexType::eUint32;
			vertex_data.mIndirectCount = (int32_t)m_resource->m_mesh.size();

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

	m_resource->m_model_info.mInvGlobalMatrix = glm::inverse(m_resource->mNodeRoot.mNodeList[0].mTransformation);


	auto e = std::chrono::system_clock::now();
	auto t = std::chrono::duration_cast<std::chrono::microseconds>(e - s);
	sDebug::Order().print(sDebug::FLAG_LOG | sDebug::SOURCE_MODEL, "[Load Complete %6.2fs] %s NodeNum = %d BoneNum = %d \n", t.count() / 1000.f / 1000.f, filename.c_str(), m_resource->mNodeRoot.mNodeList.size(), m_resource->mBone.size());

}


std::string cModel::getFilename() const
{
	return m_resource ? m_resource->m_filename : "";
}
const cModel::VertexResource* cModel::getMesh() const
{
	return m_resource ? &m_resource->m_vertex : nullptr;
}
