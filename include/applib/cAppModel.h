#pragma once
#include <memory>
#include <btrlib/cModel.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <applib/cLightPipeline.h>
#include <btrlib/cMotion.h>

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
struct PlayingAnimation
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
	AppModel(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum)
	{
		assert(!resource->mBone.empty());
		m_resource = resource;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		// node info
		{
			auto node_info = NodeInfo::createNodeInfo(resource->mNodeRoot);
			btr::BufferMemoryDescriptorEx<NodeInfo> desc;
			desc.element_num = node_info.size();
			m_nodeinfo_buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), staging.getInfo().range, node_info.data(), vector_sizeof(node_info));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(m_nodeinfo_buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_nodeinfo_buffer.getInfo().buffer, copy_info);

			auto to_render = m_nodeinfo_buffer.makeMemoryBarrier();
			to_render.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_render, {});

		}

		{
			// BoneInfo
			auto bone_info = BoneInfo::createBoneInfo(resource->mBone);
			btr::BufferMemoryDescriptorEx<BoneInfo> desc;
			desc.element_num = bone_info.size();

			auto& buffer = m_boneinfo_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), vector_sizeof(bone_info), bone_info.data(), vector_sizeof(bone_info));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);
		}

		// BoneTransform
		{
			btr::BufferMemoryDescriptorEx<mat4> desc;
			desc.element_num = resource->mBone.size() * instanceNum;

			auto& buffer = m_bonetransform_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);
		}
		// NodeTransform
		{
			btr::BufferMemoryDescriptorEx<mat4> desc;
			desc.element_num = resource->mNodeRoot.mNodeList.size() * instanceNum;

			auto& buffer = m_nodetransform_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);
		}

		// ModelInfo
		{
			btr::BufferMemoryDescriptorEx<cModel::ModelInfo> desc;
			desc.element_num = 1;

			auto& buffer = m_modelinfo_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			auto& mi = *static_cast<cModel::ModelInfo*>(staging.getMappedPtr());
			mi = resource->m_model_info;

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);

		}

		//ModelInstancingInfo
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
//			desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
			desc.frame_max = sGlobal::FRAME_MAX;
			desc.element_num = 1;
			m_instancing_info_buffer.setup(desc);
		}
		// world
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
//			desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
			desc.frame_max = sGlobal::FRAME_MAX;
			desc.element_num = instanceNum;
			m_world_buffer.setup(desc);
		}

		//BoneMap
		{
			btr::BufferMemoryDescriptorEx<s32> desc;
			desc.element_num = instanceNum;

			auto& buffer = m_bonemap_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);
		}
		// draw indirect
		{
			btr::BufferMemoryDescriptorEx<cModel::Mesh> desc;
			desc.element_num = resource->m_mesh.size();

			auto& buffer = m_draw_indirect_buffer;
			buffer = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), staging.getInfo().range, resource->m_mesh.data(), staging.getInfo().range);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);

		}

		{

			auto& buffer = m_animation_skinning_indirect_buffer;

			btr::BufferMemoryDescriptorEx<ivec3> desc;
			desc.element_num = 6;
			buffer = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			auto* group_ptr = static_cast<glm::ivec3*>(staging.getMappedPtr());
			int32_t local_size_x = 1024;
			// shaderのlocal_size_xと合わせる
			std::vector<glm::ivec3> group =
			{
				glm::ivec3(1, 1, 1),
				glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum*resource->m_model_info.mNodeNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum*resource->m_model_info.mNodeNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum*resource->m_model_info.mBoneNum + local_size_x - 1) / local_size_x, 1, 1),
			};
			memcpy_s(group_ptr, sizeof(glm::ivec3) * 6, group.data(), sizeof(glm::ivec3) * 6);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(m_animation_skinning_indirect_buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_animation_skinning_indirect_buffer.getInfo().buffer, copy_info);

			vk::BufferMemoryBarrier dispatch_indirect_barrier;
			dispatch_indirect_barrier.setBuffer(m_animation_skinning_indirect_buffer.getInfo().buffer);
			dispatch_indirect_barrier.setOffset(m_animation_skinning_indirect_buffer.getInfo().offset);
			dispatch_indirect_barrier.setSize(m_animation_skinning_indirect_buffer.getInfo().range);
			dispatch_indirect_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			dispatch_indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eDrawIndirect,
				vk::DependencyFlags(),
				{}, { dispatch_indirect_barrier }, {});
		}

		{
			auto& anim = resource->getAnimation();
			m_motion_texture = MotionTexture::createMotion(context, cmd, resource->getAnimation());

			btr::BufferMemoryDescriptorEx<AnimationInfo> desc;
			desc.element_num = anim.m_motion.size();

			auto& buffer = m_animationinfo_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			auto* staging_ptr = staging.getMappedPtr();
			for (size_t i = 0; i < anim.m_motion.size(); i++)
			{
				auto& animation = staging_ptr[i];
				animation.duration_ = (float)anim.m_motion[i]->m_duration;
				animation.ticksPerSecond_ = (float)anim.m_motion[i]->m_ticks_per_second;
			}

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);

			vk::BufferMemoryBarrier barrier;
			barrier.setBuffer(buffer.getInfo().buffer);
			barrier.setOffset(buffer.getInfo().offset);
			barrier.setSize(buffer.getInfo().range);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(),
				{}, { barrier }, {});
		}
		// PlayingAnimation
		{
			btr::BufferMemoryDescriptorEx<PlayingAnimation> desc;
			desc.element_num = instanceNum;

			auto& buffer = m_animationplay_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			auto* pa = staging.getMappedPtr();
			for (int i = 0; i < instanceNum; i++)
			{
				pa[i].playingAnimationNo = 0;
				pa[i].isLoop = true;
				pa[i].time = (float)(std::rand() % 200);
				pa[i]._p2 = 0;
			}


			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy_info);
		}

		// material index
		{
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = resource->m_mesh.size();
			m_material_index = context->m_storage_memory.allocateMemory(desc);
			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			std::vector<uint32_t> material_index(resource->m_mesh.size());
			for (size_t i = 0; i < material_index.size(); i++)
			{
				staging.getMappedPtr()[i] = resource->m_mesh[i].m_material_index;
			}


			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(m_material_index.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_material_index.getInfo().buffer, copy_info);

		}

		// material
		{
			btr::BufferMemoryDescriptorEx<MaterialBuffer> desc;
			desc.element_num = resource->m_material.size();
			m_material = context->m_storage_memory.allocateMemory(desc);
			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			auto* mb = static_cast<MaterialBuffer*>(staging.getMappedPtr());
			for (size_t i = 0; i < resource->m_material.size(); i++)
			{
				mb[i].mAmbient = resource->m_material[i].mAmbient;
				mb[i].mDiffuse = resource->m_material[i].mDiffuse;
				mb[i].mEmissive = resource->m_material[i].mEmissive;
				mb[i].mSpecular = resource->m_material[i].mSpecular;
				mb[i].mShininess = resource->m_material[i].mShininess;
			}


			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(m_material.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_material.getInfo().buffer, copy_info);
		}

		// todo 結構適当
		m_texture.resize(resource->m_material.size() * 1);
		for (size_t i = 0; i < resource->m_material.size(); i++)
		{
			auto& m = resource->m_material[i];
			m_texture[i * 1 + 0] = m.mDiffuseTex.isReady() ? m.mDiffuseTex : ResourceTexture();
		}

	}
	std::shared_ptr<cModel::Resource> m_resource;
	uint32_t m_instance_max_num;
	std::vector<MotionTexture> m_motion_texture;
	btr::BufferMemoryEx<ivec3> m_animation_skinning_indirect_buffer;

	btr::UpdateBuffer<ModelInstancingInfo> m_instancing_info_buffer;
	btr::UpdateBuffer<mat4> m_world_buffer;
	btr::BufferMemoryEx<cModel::ModelInfo> m_modelinfo_buffer;
	btr::BufferMemoryEx<NodeInfo> m_nodeinfo_buffer;
	btr::BufferMemoryEx<BoneInfo> m_boneinfo_buffer;
	btr::BufferMemoryEx<AnimationInfo> m_animationinfo_buffer;
	btr::BufferMemoryEx<PlayingAnimation> m_animationplay_buffer;
	btr::BufferMemoryEx<mat4> m_nodetransform_buffer;
	btr::BufferMemoryEx<mat4> m_bonetransform_buffer;
	btr::BufferMemoryEx<s32> m_bonemap_buffer;
	btr::BufferMemoryEx<cModel::Mesh> m_draw_indirect_buffer;

	btr::BufferMemoryEx<uint32_t> m_material_index;
	btr::BufferMemoryEx<MaterialBuffer> m_material;
	std::vector<ResourceTexture> m_texture;

// 	{ 
// 		// bufferの更新
// 		{
// 			auto frame = sGlobal::Order().getRenderFrame();
// 			int32_t model_count = m_instance_count[frame].exchange(0);
// 			if (model_count == 0)
// 			{
// 				// やることない
// 				return;
// 			}
// 			// world
// 			{
// 				auto& world = m_world_buffer;
// 				world.flushSubBuffer(model_count, 0, frame);
// 				auto& buffer = world.getBufferMemory();
// 
// 				vk::BufferMemoryBarrier to_copy_barrier;
// 				to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
// 				to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
// 				to_copy_barrier.setSize(buffer.getBufferInfo().range);
// 				to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
// 				to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
// 				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
// 
// 				vk::BufferCopy copy_info = world.update(frame);
// 				cmd.copyBuffer(world.getStagingBufferInfo().buffer, world.getBufferInfo().buffer, copy_info);
// 
// 				vk::BufferMemoryBarrier to_shader_read_barrier;
// 				to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
// 				to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
// 				to_shader_read_barrier.setSize(copy_info.size);
// 				to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
// 				to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
// 				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
// 			}
// 			{
// 				auto& instancing = m_instancing_info_buffer;
// 				ModelInstancingInfo info;
// 				info.mInstanceAliveNum = model_count;
// 				info.mInstanceMaxNum = m_instance_max_num;
// 				info.mInstanceNum = 0;
// 				instancing.subupdate(&info, 1, 0, frame);
// 
// 				auto& buffer = instancing.getBufferMemory();
// 				vk::BufferMemoryBarrier to_copy_barrier;
// 				to_copy_barrier.setBuffer(buffer.getBufferInfo().buffer);
// 				to_copy_barrier.setOffset(buffer.getBufferInfo().offset);
// 				to_copy_barrier.setSize(buffer.getBufferInfo().range);
// 				to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
// 				to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
// 				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
// 
// 				vk::BufferCopy copy_info = instancing.update(frame);
// 				cmd.copyBuffer(instancing.getStagingBufferInfo().buffer, instancing.getBufferInfo().buffer, copy_info);
// 
// 				vk::BufferMemoryBarrier to_shader_read_barrier;
// 				to_shader_read_barrier.setBuffer(buffer.getBufferInfo().buffer);
// 				to_shader_read_barrier.setOffset(buffer.getBufferInfo().offset);
// 				to_shader_read_barrier.setSize(buffer.getBufferInfo().range);
// 				to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
// 				to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
// 				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
// 			}
// 
// 		}
// //		cmd.executeCommands(m_execute_cmd[context->getGPUFrame()].get());
// 	}
};