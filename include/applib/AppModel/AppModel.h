#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/cModel.h>
#include <btrlib/cMotion.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <applib/App.h>

struct ModelInstancingInfo {
	s32 mInstanceMaxNum;		//!< 出せるモデルの量
	s32 mInstanceAliveNum;		//!< 生きているモデルの量
	s32 mInstanceNum;			//!< 実際に描画する数
	s32 _p[1];
};
struct BoneInfo
{
	s32 mNodeIndex;
	s32 _p[3];
	glm::mat4 mBoneOffset;

	static std::vector<BoneInfo> createBoneInfo(const std::vector<cModel::Bone>& bone)
	{
		std::vector<BoneInfo> bo(bone.size());
		for (size_t i = 0; i < bone.size(); i++) {
			bo[i].mBoneOffset = bone[i].mOffset;
			bo[i].mNodeIndex = bone[i].mNodeIndex;
		}
		return bo;
	}
};
struct NodeInfo
{
	int32_t		mNodeNo;
	int32_t		mParent;
	int32_t		mBoneIndex;
	int32_t		m_depth;	//!< RootNodeからの深さ

	static std::vector<NodeInfo> createNodeInfo(const RootNode& rootnode)
	{
		std::vector<NodeInfo> nodeBuffer;
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

private:
	static void createNodeInfoRecurcive(const RootNode& rootnode, const Node& node, std::vector<NodeInfo>& nodeBuffer, int parentIndex)
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


};
struct AnimationInfo
{
	float duration_;
	float ticksPerSecond_;
	int numInfo_;
	int offsetInfo_;
};
struct AnimationWorker
{
	int		playingAnimationNo;	//!< モーション
	float	time;				//!< モーションの再生時間 
	int		_p2;	//!< 今再生しているモーションデータのインデックス
	int		isLoop;				//!<
};
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

struct AppModelContext
{
	enum {
		DESCRIPTOR_ALBEDO_TEXTURE_NUM = 16,
	};
	enum DescriptorLayout
	{
		DescriptorLayout_Model,
		DescriptorLayout_Render,
		DescriptorLayout_Update,
		DescriptorLayout_Num,
	};
	AppModelContext(const std::shared_ptr<btr::Context>& context)
	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		{
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
			descriptor_layout_info.setBindingCount(array_length(binding));
			descriptor_layout_info.setPBindings(binding);
			m_descriptor_set_layout[DescriptorLayout_Model] = context->m_device.createDescriptorSetLayoutUnique(descriptor_layout_info);
		}
		{
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, DESCRIPTOR_ALBEDO_TEXTURE_NUM, stage),
			};
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
			descriptor_layout_info.setBindingCount(array_length(binding));
			descriptor_layout_info.setPBindings(binding);
			m_descriptor_set_layout[DescriptorLayout_Render] = context->m_device.createDescriptorSetLayoutUnique(descriptor_layout_info);
		}
		{
			vk::DescriptorSetLayoutBinding binding[] =
			{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
			descriptor_layout_info.setBindingCount(array_length(binding));
			descriptor_layout_info.setPBindings(binding);
			m_descriptor_set_layout[DescriptorLayout_Update] = context->m_device.createDescriptorSetLayoutUnique(descriptor_layout_info);
		}
	}
	std::array<vk::UniqueDescriptorSetLayout, DescriptorLayout_Num> m_descriptor_set_layout;

	vk::DescriptorSetLayout getLayout(DescriptorLayout layout)const { return m_descriptor_set_layout[layout].get(); }
};

struct AppModel
{

	AppModel(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<AppModelContext>& appmodel_context, const std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum);

	std::shared_ptr<cModel::Resource> m_resource;
	uint32_t m_instance_max_num;

	std::vector<MotionTexture> m_motion_texture;
	btr::BufferMemoryEx<uvec3> b_animation_indirect;

	btr::BufferMemoryEx<cModel::ModelInfo> b_model_info;
	btr::BufferMemoryEx<NodeInfo> b_node_info;
	btr::BufferMemoryEx<BoneInfo> b_bone_info;

	btr::BufferMemoryEx<ModelInstancingInfo> b_model_instancing_info;
	btr::BufferMemoryEx<cModel::Mesh> b_draw_indirect;
	btr::BufferMemoryEx<u32> b_instance_map;

	btr::BufferMemoryEx<AnimationInfo> b_animation_info;
	btr::BufferMemoryEx<AnimationWorker> b_animation_work;

	//! material
	btr::BufferMemoryEx<uint32_t> b_material_index;
	btr::BufferMemoryEx<MaterialBuffer> b_material;
	std::vector<ResourceTexture> m_albedo_texture;

	//! 作業用バッファ
	btr::BufferMemoryEx<mat4> b_node_transform;
	btr::BufferMemoryEx<mat4> b_bone_transform;

	//! 好きにしていいバッファ
	btr::BufferMemoryEx<mat4> b_world;

	struct AppModelRender : public Drawable
	{
		std::shared_ptr<cModel::Resource> m_data;
		vk::DescriptorBufferInfo m_indirect_buffer;
		void draw(vk::CommandBuffer cmd)const override
		{
			cmd.bindVertexBuffers(0, m_data->m_mesh_resource.v_position.getInfo().buffer, m_data->m_mesh_resource.v_position.getInfo().offset);
			cmd.bindVertexBuffers(1, m_data->m_mesh_resource.v_normal.getInfo().buffer, m_data->m_mesh_resource.v_normal.getInfo().offset);
			cmd.bindVertexBuffers(2, m_data->m_mesh_resource.v_texcoord.getInfo().buffer, m_data->m_mesh_resource.v_texcoord.getInfo().offset);
			cmd.bindVertexBuffers(3, m_data->m_mesh_resource.v_bone_id.getInfo().buffer, m_data->m_mesh_resource.v_bone_id.getInfo().offset);
			cmd.bindVertexBuffers(4, m_data->m_mesh_resource.v_bone_weight.getInfo().buffer, m_data->m_mesh_resource.v_bone_weight.getInfo().offset);
			cmd.bindIndexBuffer(m_data->m_mesh_resource.v_index.getInfo().buffer, m_data->m_mesh_resource.v_index.getInfo().offset, m_data->m_mesh_resource.mIndexType);
			cmd.drawIndexedIndirect(m_indirect_buffer.buffer, m_indirect_buffer.offset, m_data->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));
		}
	};
	AppModelRender m_render;

	enum DescriptorSet
	{
		DescriptorSet_Model,
		DescriptorSet_Render,
		DescriptorSet_Update,
		DescriptorSet_Num,
	};
	std::array<vk::UniqueDescriptorSet, DescriptorSet_Num> m_descriptor_set;

	vk::DescriptorSet getDescriptorSet(DescriptorSet layout)const { return m_descriptor_set[layout].get(); }

};
