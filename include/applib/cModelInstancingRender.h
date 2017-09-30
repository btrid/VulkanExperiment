#pragma once
#include <memory>
#include <btrlib/cModel.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <applib/cLightPipeline.h>

struct cModelInstancingPipeline;

namespace model
{
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
}


class ModelInstancingRender
{
public:
	struct AnimationInfo
	{
		//	int animationNo_;
		float duration_;
		float ticksPerSecond_;
		int numInfo_;
		int offsetInfo_;
	};
	struct PlayingAnimation
	{
		int		playingAnimationNo;	//!< モーション
		float	time;				//!< モーションの再生時間 
		int		currentMotionInfoIndex;	//!< 今再生しているモーションデータのインデックス
		int		isLoop;				//!<
	};


	struct ModelInstancingInfo {
		s32 mInstanceMaxNum;		//!< 出せるモデルの量
		s32 mInstanceAliveNum;		//!< 生きているモデルの量
		s32 mInstanceNum;			//!< 実際に描画する数
		s32 _p[1];
	};
public:

	std::shared_ptr<cModel::Resource> m_resource;

	enum DescriptorSet
	{
		DESCRIPTOR_SET_MODEL,
		DESCRIPTOR_SET_ANIMATION,
	};
	std::vector<vk::UniqueDescriptorSet> m_descriptor_set;
	vk::UniqueDescriptorSet m_draw_descriptor_set_per_model;
	std::vector<vk::UniqueDescriptorSet> m_compute_descriptor_set;
	std::shared_ptr<MaterialModule> m_material;

	enum ModelStorageBuffer : s32
	{
		MODEL_INFO,
		MODEL_INSTANCING_INFO,
		NODE_INFO,
		BONE_INFO,
		ANIMATION_INFO,
		PLAYING_ANIMATION,
		NODE_TRANSFORM,
		BONE_TRANSFORM,
		BONE_MAP,	//!< instancingの際のBoneの参照先
//		WORLD,
		NUM,
	};
	struct InstancingResource
	{
		uint32_t m_instance_max_num;
		std::vector<MotionTexture> m_motion_texture;
		btr::BufferMemory m_compute_indirect_buffer;
		btr::UpdateBuffer<mat4> m_world;
		btr::BufferMemory m_instancing_info;
		std::array<btr::BufferMemory, static_cast<s32>(ModelStorageBuffer::NUM)> m_storage_buffer;
		const btr::BufferMemory& getBuffer(ModelStorageBuffer buffer)const { return m_storage_buffer[static_cast<s32>(buffer)]; }
		btr::BufferMemory& getBuffer(ModelStorageBuffer buffer) { return m_storage_buffer[static_cast<s32>(buffer)]; }

	};
	std::unique_ptr<InstancingResource> m_resource_instancing;


	struct InstanceResource
	{
		mat4 m_world;
	};
	std::array<std::atomic_uint32_t,sGlobal::FRAME_MAX> m_instance_count;
	void addModel(const InstanceResource& data) { addModel(&data, 1); }
	void addModel(const InstanceResource* data, uint32_t num);
public:
	void setup(std::shared_ptr<btr::Context>& loader, std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum);

	void setup(cModelInstancingPipeline& pipeline);
	void execute(cModelInstancingPipeline& pipeline, vk::CommandBuffer& cmd);
	void draw(cModelInstancingPipeline& pipeline, vk::CommandBuffer& cmd);

protected:
private:
};
