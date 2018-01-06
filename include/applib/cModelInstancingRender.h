#pragma once
#include <memory>
#include <btrlib/cModel.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <applib/cLightPipeline.h>
#include <btrlib/cMotion.h>
#include <btrlib/Material.h>

struct ModelInstancingModule : public InstancingAnimationModule, public InstancingModule
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
	struct AnimationInfo
	{
		float duration_;
		float ticksPerSecond_;
		int _p2;
		int _p3;
	};
	struct PlayingAnimation
	{
		int		playingAnimationNo;	//!< モーション
		float	time;				//!< モーションの再生時間 
		int		_p2;	//!< 今再生しているモーションデータのインデックス
		int		isLoop;				//!<
	};

	struct ModelInstancingInfo {
		s32 mInstanceMaxNum;		//!< 出せるモデルの量
		s32 mInstanceAliveNum;		//!< 生きているモデルの量
		s32 mInstanceNum;			//!< 実際に描画する数
		s32 _p[1];
	};
	enum ModelStorageBuffer : s32
	{
		MODEL_INFO,
		NODE_INFO,
		BONE_INFO,
		ANIMATION_INFO,
		PLAYING_ANIMATION,
		NODE_TRANSFORM,
		BONE_TRANSFORM,
		BONE_MAP,	//!< instancingの際のBoneの参照先
		DRAW_INDIRECT,
		NUM,
	};

	uint32_t m_instance_max_num;
	std::vector<MotionTexture> m_motion_texture;
	btr::BufferMemory m_compute_indirect_buffer;

	btr::UpdateBuffer<mat4> m_world_buffer;
	btr::UpdateBuffer<ModelInstancingInfo> m_instancing_info_buffer;

	vk::UniqueDescriptorSet m_animation_descriptor_set;
	std::vector<vk::UniqueCommandBuffer> m_execute_cmd;

	std::array<btr::BufferMemory, static_cast<s32>(ModelStorageBuffer::NUM)> m_storage_buffer;
	const btr::BufferMemory& getBuffer(ModelStorageBuffer buffer)const { return m_storage_buffer[static_cast<s32>(buffer)]; }
	btr::BufferMemory& getBuffer(ModelStorageBuffer buffer) { return m_storage_buffer[static_cast<s32>(buffer)]; }

	struct InstanceResource
	{
		mat4 m_world;
	};
	std::array<std::atomic_uint32_t, sGlobal::FRAME_MAX> m_instance_count;
	void addModel(const InstanceResource& data) { addModel(&data, 1); }
	void addModel(const InstanceResource* data, uint32_t num);

	virtual vk::DescriptorBufferInfo getBoneBuffer()const override { return getBuffer(BONE_TRANSFORM).getBufferInfo(); }
	virtual void animationUpdate() override {}
	virtual void animationExecute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd) override 
	{ 
		// bufferの更新
		{
			auto frame = sGlobal::Order().getRenderFrame();
			int32_t model_count = m_instance_count[frame].exchange(0);
			if (model_count == 0)
			{
				// やることない
				return;
			}
			// world
			{
				auto& world = m_world_buffer;
				world.flushSubBuffer(model_count, 0, frame);
				auto& buffer = world.getBufferMemory();

				vk::BufferMemoryBarrier to_copy_barrier;
				to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
				to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
				to_copy_barrier.setSize(buffer.getBufferInfo().range);
				to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

				vk::BufferCopy copy_info = world.update(frame);
				cmd.copyBuffer(world.getStagingBufferInfo().buffer, world.getBufferInfo().buffer, copy_info);

				vk::BufferMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
				to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
				to_shader_read_barrier.setSize(copy_info.size);
				to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
			}
			{
				auto& instancing = m_instancing_info_buffer;
				ModelInstancingInfo info;
				info.mInstanceAliveNum = model_count;
				info.mInstanceMaxNum = m_instance_max_num;
				info.mInstanceNum = 0;
				instancing.subupdate(&info, 1, 0, frame);

				auto& buffer = instancing.getBufferMemory();
				vk::BufferMemoryBarrier to_copy_barrier;
				to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
				to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
				to_copy_barrier.setSize(buffer.getBufferInfo().range);
				to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

				vk::BufferCopy copy_info = instancing.update(frame);
				cmd.copyBuffer(instancing.getStagingBufferInfo().buffer, instancing.getBufferInfo().buffer, copy_info);

				vk::BufferMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
				to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
				to_shader_read_barrier.setSize(buffer.getBufferInfo().range);
				to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
			}

		}
		cmd.executeCommands(m_execute_cmd[context->getGPUFrame()].get());
	}

//	virtual vk::DescriptorBufferInfo getModelInfoBuffer()const override { return getBuffer(MODEL_INFO).getBufferInfo(); }
//	virtual vk::DescriptorBufferInfo getModelInstancingBuffer()const override { return m_instancing_info_buffer.getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getAnimationInfoBuffer()const override { return getBuffer(ANIMATION_INFO).getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getPlayingAnimationBuffer()const override { return getBuffer(PLAYING_ANIMATION).getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getNodeBuffer()const override { return getBuffer(NODE_TRANSFORM).getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getBoneMap()const override { return getBuffer(BONE_MAP).getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getNodeInfoBuffer()const override { return getBuffer(NODE_INFO).getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getBoneInfoBuffer()const override { return getBuffer(BONE_INFO).getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getWorldBuffer()const override { return m_world_buffer.getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getDrawIndirect()const override { return getBuffer(DRAW_INDIRECT).getBufferInfo(); }
	virtual const std::vector<MotionTexture>& getMotionTexture()const override { return m_motion_texture; }

	virtual vk::DescriptorBufferInfo getModelInfo()const override { return getBuffer(MODEL_INFO).getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getInstancingInfo()const override { return m_instancing_info_buffer.getBufferMemory().getBufferInfo(); }

};
struct ModelInstancingRender
{
	vk::UniqueDescriptorSet m_model_descriptor_set;
	std::vector<vk::UniqueCommandBuffer> m_draw_cmd;

};

class ModelInstancingModel
{
public:
	std::shared_ptr<cModel::Resource> m_resource;

	std::shared_ptr<MaterialModule> m_material;
	std::shared_ptr<ModelInstancingModule> m_instancing;
	std::shared_ptr<ModelInstancingRender> m_render;


public:

	void execute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd);
	void draw(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd);

protected:
private:
};
