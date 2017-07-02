#pragma once
#include <btrlib/Define.h>
#include <btrlib/BufferMemory.h>
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
		float			mShininess;
		float			__p;
		float			__p1;
		float			__p2;
	};

	cModelRenderPrivate();
	~cModelRenderPrivate();

	std::shared_ptr<cModel::Resource> m_model_resource;
	std::vector<glm::mat4> m_node_buffer;
	btr::UpdateBufferEx m_bone_buffer;
	MotionPlayList m_playlist;

	ModelTransform m_model_transform;

	vk::DescriptorSet m_draw_descriptor_set_per_model;
	btr::AllocatedMemory m_material;
	std::vector<vk::DescriptorSet> m_draw_descriptor_set_per_mesh;
	std::vector<vk::DescriptorSet> m_compute_descriptor_set;

	void setup(std::shared_ptr<btr::Loader>& loader, std::shared_ptr<cModel::Resource> resource)
	{
		m_model_resource = resource;
		m_node_buffer.resize(m_model_resource->m_model_info.mNodeNum);

		{
			// bone
			btr::UpdateBufferExDescriptor desc;
			desc.alloc_size = (uint32_t)(m_model_resource->mBone.size() * sizeof(glm::mat4));
			desc.device_memory = loader->m_storage_memory;
			desc.staging_memory = loader->m_staging_memory;
			desc.frame_max = sGlobal::FRAME_MAX;
			m_bone_buffer.setup(desc);
		}
		{
			// material
			btr::BufferMemory::Descriptor staging_desc;
			staging_desc.size = m_model_resource->m_material.size() * sizeof(MaterialBuffer);
			staging_desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
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
			loader->m_cmd.copyBuffer(staging_material.getBufferInfo().buffer, m_material.getBufferInfo().buffer, copy_info);
		}

		{}
	}
	void execute(std::shared_ptr<btr::Executer>& executer)
	{
	}
	void draw(vk::CommandBuffer cmd)
	{
	}
	void updateBoneTransform(uint32_t node_index)
	{
		// シェーダに送るデータを更新
		auto* node = m_model_resource->mNodeRoot.getNodeByIndex(node_index);
		int32_t bone_index =node->m_bone_index;
		if (bone_index >= 0)
		{
			auto m = m_node_buffer[node_index] * m_model_resource->mBone[bone_index].mOffset;
			m_bone_buffer.subupdate(&m, sizeof(glm::mat4), bone_index* sizeof(glm::mat4));
		}

		for (auto& n : node->mChildren) {
			updateBoneTransform(n);
		}


	}

	void updateNodeTransform(uint32_t node_index, const glm::mat4& parentMat)
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
//				transformMatrix = parentMat * glm::scale(scale_value) * glm::toMat4(rotate_value) * glm::translate(translate_value);
				transformMatrix = parentMat * glm::scale(scale_value) * glm::translate(translate_value)* glm::toMat4(rotate_value);
				m_node_buffer[node_index] = transformMatrix;
			}
		}
		for (auto& n : node->mChildren) {
			updateNodeTransform(n, m_node_buffer[node_index]);
		}
	}

	void setup(cModelPipeline& renderer);
	void execute(cModelPipeline& renderer, vk::CommandBuffer& cmd);
	void draw(cModelPipeline& renderer, vk::CommandBuffer& cmd);

};
