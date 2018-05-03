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

};
struct AnimationInfo
{
	float duration_;
	float ticksPerSecond_;
	int _p2;
	int _p3;
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


struct AppModel
{
	struct DescriptorSet : public SingletonEx<DescriptorSet>
	{
		enum {
			DESCRIPTOR_ALBEDO_TEXTURE_NUM = 16,
		};

		DescriptorSet(const std::shared_ptr<btr::Context>& context)
		{
			auto stage = vk::ShaderStageFlagBits::eVertex| vk::ShaderStageFlagBits::eFragment| vk::ShaderStageFlagBits::eCompute;
			std::vector<vk::DescriptorSetLayoutBinding> binding =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(2),

				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(10),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(11),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorCount(DESCRIPTOR_ALBEDO_TEXTURE_NUM)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setBinding(12),

				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setBinding(20),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(21),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(22),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(23),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(24),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(25),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(26),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(27),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(28),

			};
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
			descriptor_layout_info.setBindingCount((uint32_t)binding.size());
			descriptor_layout_info.setPBindings(binding.data());
			m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(descriptor_layout_info);
//			m_descriptor_pool = createDescriptorPool(context, binding, 30);
//			m_context = context;
		}
//		std::shared_ptr<btr::Context> m_context;
	protected:
		vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
//		vk::UniqueDescriptorPool m_descriptor_pool;
	public:
		vk::DescriptorSetLayout getLayout()const { return m_descriptor_set_layout.get(); }
	protected:
//		vk::DescriptorPool getPool()const { return m_descriptor_pool.get(); }
	};

	AppModel(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum);

	std::shared_ptr<cModel::Resource> m_resource;
	uint32_t m_instance_max_num;
	std::vector<MotionTexture> m_motion_texture;
	btr::BufferMemoryEx<uvec3> b_animation_indirect;

	btr::BufferMemoryEx<cModel::ModelInfo> b_model_info;
	btr::BufferMemoryEx<ModelInstancingInfo> b_model_instancing_info;

	btr::BufferMemoryEx<mat4> b_worlds;
	btr::BufferMemoryEx<NodeInfo> b_node_info;
	btr::BufferMemoryEx<BoneInfo> b_bone_info;
	btr::BufferMemoryEx<AnimationInfo> b_animation_info;
	btr::BufferMemoryEx<AnimationWorker> b_animation_work;
	btr::BufferMemoryEx<mat4> b_node_transforms;
	btr::BufferMemoryEx<mat4> b_bone_transforms;
	btr::BufferMemoryEx<u32> b_instance_map;
	btr::BufferMemoryEx<cModel::Mesh> b_draw_indirect;

	btr::BufferMemoryEx<uint32_t> m_material_index;
	btr::BufferMemoryEx<MaterialBuffer> m_material;
	std::vector<ResourceTexture> m_texture;

	struct AppModelRender : public Drawable
	{
		btr::BufferMemory m_vertex_buffer;
		btr::BufferMemory m_index_buffer;
		vk::DescriptorBufferInfo m_indirect_buffer;
		vk::IndexType m_index_type;
		int32_t m_indirect_count; //!< メッシュの数
		int32_t m_indirect_stride; //!< 
		void draw(vk::CommandBuffer cmd)const override
		{
			cmd.bindVertexBuffers(0, m_vertex_buffer.getInfo().buffer, m_vertex_buffer.getInfo().offset);
			cmd.bindIndexBuffer(m_index_buffer.getInfo().buffer, m_index_buffer.getInfo().offset, m_index_type);
			cmd.drawIndexedIndirect(m_indirect_buffer.buffer, m_indirect_buffer.offset, m_indirect_count, m_indirect_stride);
		}
	};
	AppModelRender m_render;

	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }

};
