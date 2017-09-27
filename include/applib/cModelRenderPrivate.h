#pragma once
#include <btrlib/Define.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cModel.h>
#include <applib/cModelRender.h>

struct cModelPipeline;

struct cModelRenderPrivate
{
	struct MaterialBuffer {
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		glm::vec4		mEmissive;
		uint32_t		u_albedo_texture;
		uint32_t		u_ambient_texture;
		uint32_t		u_specular_texture;
		uint32_t		u_emissive_texture;
		float			mShininess;
		float			__p;
		float			__p1;
		float			__p2;
	};

	cModelRenderPrivate();
	~cModelRenderPrivate();

	std::shared_ptr<cModel::Resource> m_model_resource;
	std::vector<btr::BufferMemory> m_bone_buffer_staging;

	std::vector<vk::UniqueCommandBuffer> m_transfer_cmd;
	std::vector<vk::UniqueCommandBuffer> m_graphics_cmd;

	ModelTransform m_model_transform;
	MotionPlayList m_playlist;

	vk::UniqueDescriptorSet m_draw_descriptor_set_per_model;

	btr::BufferMemory m_bone_buffer;
	btr::BufferMemory m_material_index;
	btr::BufferMemory m_material;

	void setup(std::shared_ptr<btr::Loader>& loader, std::shared_ptr<cModel::Resource> resource)
	{
		auto cmd = loader->m_cmd_pool->allocCmdTempolary(0);

		m_model_resource = resource;

		{
			// bone
			btr::UpdateBufferExDescriptor desc;
			desc.alloc_size = (uint32_t)(m_model_resource->mBone.size() * sizeof(glm::mat4));
			desc.device_memory = loader->m_storage_memory;
			desc.staging_memory = loader->m_staging_memory;
			desc.frame_max = sGlobal::FRAME_MAX;
			m_bone_buffer_staging.resize(sGlobal::FRAME_MAX);
			for (auto& buffer : m_bone_buffer_staging)
			{
				btr::AllocatedMemory::Descriptor desc;
				desc.size = m_model_resource->mBone.size() * sizeof(glm::mat4);
				buffer = loader->m_staging_memory.allocateMemory(desc);
			}
			{
				btr::AllocatedMemory::Descriptor desc;
				desc.size = m_model_resource->mBone.size() * sizeof(glm::mat4);
				m_bone_buffer = loader->m_storage_memory.allocateMemory(desc);
			}
		}

		// material index
		{
			btr::AllocatedMemory::Descriptor staging_desc;
			staging_desc.size = m_model_resource->m_mesh.size() * sizeof(uint32_t);
			staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(staging_desc);

			std::vector<uint32_t> material_index(m_model_resource->m_mesh.size());
			for (size_t i = 0; i < material_index.size(); i++)
			{
				staging.getMappedPtr<uint32_t>()[i] = m_model_resource->m_mesh[i].m_material_index;
			}
			
			m_material_index = loader->m_storage_memory.allocateMemory(material_index.size() * sizeof(uint32_t));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(m_material_index.getBufferInfo().offset);
			cmd->copyBuffer(staging.getBufferInfo().buffer, m_material_index.getBufferInfo().buffer, copy_info);

		}

		// material
		{
			btr::AllocatedMemory::Descriptor staging_desc;
			staging_desc.size = m_model_resource->m_material.size() * sizeof(MaterialBuffer);
			staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_material = loader->m_staging_memory.allocateMemory(staging_desc);
			auto* mb = static_cast<MaterialBuffer*>(staging_material.getMappedPtr());
			for (size_t i = 0; i < m_model_resource->m_material.size(); i++)
			{
				mb[i].mAmbient = m_model_resource->m_material[i].mAmbient;
				mb[i].mDiffuse = m_model_resource->m_material[i].mDiffuse;
				mb[i].mEmissive = m_model_resource->m_material[i].mEmissive;
				mb[i].mSpecular = m_model_resource->m_material[i].mSpecular;
				mb[i].mShininess = m_model_resource->m_material[i].mShininess;
			}

			m_material = loader->m_storage_memory.allocateMemory(m_model_resource->m_material.size() * sizeof(MaterialBuffer));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_material.getBufferInfo().range);
			copy_info.setSrcOffset(staging_material.getBufferInfo().offset);
			copy_info.setDstOffset(m_material.getBufferInfo().offset);
			cmd->copyBuffer(staging_material.getBufferInfo().buffer, m_material.getBufferInfo().buffer, copy_info);
		}
	}
	void work()
	{
		m_playlist.execute();

		std::vector<glm::mat4> node_buffer(m_model_resource->m_model_info.mNodeNum);
		updateNodeTransform(0, m_model_transform.calcGlobal()*m_model_transform.calcLocal(), node_buffer);
		updateBoneTransform(node_buffer);
	}

	void updateBoneTransform(const std::vector<glm::mat4>& node_buffer)
	{
		// シェーダに送るデータを更新
		auto* ptr = m_bone_buffer_staging[sGlobal::Order().getCPUFrame()].getMappedPtr<glm::mat4>();
		for (size_t i = 0; i < m_model_resource->mNodeRoot.mNodeList.size(); i++)
		{
			auto* node = m_model_resource->mNodeRoot.getNodeByIndex(i);
			int32_t bone_index = node->m_bone_index;
			if (bone_index >= 0)
			{
				auto m = node_buffer[i] * m_model_resource->mBone[bone_index].mOffset;
				ptr[bone_index] = m;
			}
		}

	}
	void updateNodeTransform(uint32_t node_index, const glm::mat4& parentMat, std::vector<glm::mat4>& node_buffer)
	{
		auto* node = m_model_resource->mNodeRoot.getNodeByIndex(node_index);
		auto& work = m_playlist.m_work[0];
		auto target_time = m_playlist.m_work[0].m_time;
		auto& anim = m_playlist.m_work[0].m_motion;
		auto transformMatrix = parentMat;
		if (anim)
		{
			cMotion::NodeMotion* nodeAnim = nullptr;
			for (auto& a : anim->m_data)
			{
				if (a.m_nodename == node->mName)
				{
					nodeAnim = &a;
					break;
				}
			}

			if (nodeAnim)
			{
				//@Todo Index0から見てるので遅い
				size_t pos_index = 0;
				size_t rot_index = 0;
				size_t scale_index = 0;
				auto translate_value = nodeAnim->m_translate[nodeAnim->m_translate.size() - 1].m_data;
				auto rotate_value = nodeAnim->m_rotate[nodeAnim->m_rotate.size() - 1].m_data;
				auto scale_value = nodeAnim->m_scale[nodeAnim->m_scale.size() - 1].m_data;
				for (; pos_index < nodeAnim->m_translate.size() - 1; pos_index++) {
					if (nodeAnim->m_translate[pos_index+1].m_time >= target_time) {
						float rate = (target_time - nodeAnim->m_translate[pos_index].m_time) / (nodeAnim->m_translate[pos_index + 1].m_time - nodeAnim->m_translate[pos_index].m_time);
						translate_value = glm::lerp(nodeAnim->m_translate[pos_index].m_data, nodeAnim->m_translate[pos_index + 1].m_data, rate);
						break;
					}
				}
				for (; rot_index < nodeAnim->m_rotate.size() - 1; rot_index++) {
					if (nodeAnim->m_rotate[rot_index + 1].m_time >= target_time) {
						float rate = (target_time - nodeAnim->m_rotate[rot_index].m_time) / (nodeAnim->m_rotate[rot_index + 1].m_time - nodeAnim->m_rotate[rot_index].m_time);
						rotate_value = glm::lerp(nodeAnim->m_rotate[rot_index].m_data, nodeAnim->m_rotate[rot_index + 1].m_data, rate);
						break;
					}
				}
				for (; scale_index < nodeAnim->m_scale.size() - 1; scale_index++) {
					if (nodeAnim->m_scale[scale_index + 1].m_time >= target_time) {
						float rate = (target_time - nodeAnim->m_scale[scale_index].m_time) / (nodeAnim->m_scale[scale_index + 1].m_time - nodeAnim->m_scale[scale_index].m_time);
						scale_value = glm::lerp(nodeAnim->m_scale[scale_index].m_data, nodeAnim->m_scale[scale_index + 1].m_data, rate);
						break;
					}
				}
				transformMatrix = parentMat * glm::scale(scale_value) * glm::translate(translate_value)* glm::toMat4(rotate_value);
				node_buffer[node_index] = transformMatrix;
			}
		}
		for (auto& n : node->mChildren) {
			updateNodeTransform(n, node_buffer[node_index], node_buffer);
		}
	}

	void setup(std::shared_ptr<btr::Executer>& executer, cModelPipeline& renderer);
	void execute(std::shared_ptr<btr::Executer>& executer, vk::CommandBuffer& cmd);
	void draw(std::shared_ptr<btr::Executer>& executer, vk::CommandBuffer& cmd);

};
