#pragma once
#include <memory>
#include <btrlib/cModel.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <applib/cLightPipeline.h>
#include <btrlib/cMotion.h>
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
		m_instance_max_num = instanceNum;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		// node info
		{
			auto node_info = NodeInfo::createNodeInfo(resource->mNodeRoot);
			btr::BufferMemoryDescriptorEx<NodeInfo> desc;
			desc.element_num = node_info.size();
			m_node_info_buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), staging.getInfo().range, node_info.data(), vector_sizeof(node_info));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getInfo().range);
			copy_info.setSrcOffset(staging.getInfo().offset);
			copy_info.setDstOffset(m_node_info_buffer.getInfo().offset);
			cmd.copyBuffer(staging.getInfo().buffer, m_node_info_buffer.getInfo().buffer, copy_info);

			auto to_render = m_node_info_buffer.makeMemoryBarrier();
			to_render.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_render, {});

		}

		{
			// BoneInfo
			auto bone_info = BoneInfo::createBoneInfo(resource->mBone);
			btr::BufferMemoryDescriptorEx<BoneInfo> desc;
			desc.element_num = bone_info.size();

			auto& buffer = m_bone_info_buffer;
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

			auto& buffer = m_bone_transform_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);
		}
		// NodeTransform
		{
			btr::BufferMemoryDescriptorEx<mat4> desc;
			desc.element_num = resource->mNodeRoot.mNodeList.size() * instanceNum;

			auto& buffer = m_node_transform_buffer;
			buffer = context->m_storage_memory.allocateMemory(desc);
		}

		// ModelInfo
		{
			btr::BufferMemoryDescriptorEx<cModel::ModelInfo> desc;
			desc.element_num = 1;

			auto& buffer = m_model_info_buffer;
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
			btr::BufferMemoryDescriptorEx<ModelInstancingInfo> desc;
			desc.element_num = 1;

			m_instancing_info_buffer = context->m_storage_memory.allocateMemory(desc);
		}
		// world
		{
			btr::BufferMemoryDescriptorEx<mat4> desc;
			desc.element_num = instanceNum;

			m_world_buffer = context->m_storage_memory.allocateMemory(desc);
		}

		//BoneMap 改め InstanceMap
		{
			btr::BufferMemoryDescriptorEx<u32> desc;
			desc.element_num = instanceNum;

			auto& buffer = m_instance_map_buffer;
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

			btr::BufferMemoryDescriptorEx<uvec3> desc;
			desc.element_num = 6;
			buffer = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			auto* group_ptr = static_cast<glm::uvec3*>(staging.getMappedPtr());
			int32_t local_size_x = 1024;
			uvec3 local_size(1024, 1, 1);
			auto nc = (local_size_x / resource->m_model_info.mNodeNum) * resource->m_model_info.mNodeNum;
			auto node_count = instanceNum * resource->m_model_info.mNodeNum / nc + 1; // @todo 正しい計算 +1は無駄な時ある？
			uvec3 group[] =
			{
				uvec3(1, 1, 1),
				app::calcDipatchGroups(uvec3(instanceNum, 1, 1), local_size),
				app::calcDipatchGroups(uvec3(instanceNum*resource->m_model_info.mNodeNum, 1, 1), local_size),
				uvec3(1, node_count, 1),
				app::calcDipatchGroups(uvec3(instanceNum, 1, 1), local_size),
				app::calcDipatchGroups(uvec3(instanceNum*resource->m_model_info.mBoneNum, 1, 1), local_size),
			};
			memcpy_s(group_ptr, sizeof(group), group, sizeof(group));

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

		m_render.m_vertex_buffer = resource->m_mesh_resource.m_vertex_buffer;
		m_render.m_index_buffer = resource->m_mesh_resource.m_index_buffer;
		m_render.m_index_type = resource->m_mesh_resource.mIndexType;
		m_render.m_indirect_buffer = m_draw_indirect_buffer.getInfo();
		m_render.m_indirect_count = resource->m_mesh_resource.mIndirectCount;
		m_render.m_indirect_stride = sizeof(cModel::Mesh);

		// initialize
		{
			{
				ModelInstancingInfo info;
				info.mInstanceAliveNum = m_instance_max_num;
				info.mInstanceMaxNum = m_instance_max_num;
				info.mInstanceNum = 0;

				btr::BufferMemoryDescriptorEx<ModelInstancingInfo> desc;
				desc.element_num = 1;
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				*staging.getMappedPtr() = info;

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(m_instancing_info_buffer.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, m_instancing_info_buffer.getInfo().buffer, copy);
			}

			{
				btr::BufferMemoryDescriptorEx<mat4> desc;
				desc.element_num = m_instance_max_num;
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				for (uint32_t i = 0; i < m_instance_max_num; i++)
				{
					*staging.getMappedPtr(i) = glm::scale(glm::translate(glm::ballRand(700.f)), vec3(0.2f));
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(m_world_buffer.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, m_world_buffer.getInfo().buffer, copy);
			}
		}

	}
	std::shared_ptr<cModel::Resource> m_resource;
	uint32_t m_instance_max_num;
	std::vector<MotionTexture> m_motion_texture;
	btr::BufferMemoryEx<uvec3> m_animation_skinning_indirect_buffer;

	btr::BufferMemoryEx<ModelInstancingInfo> m_instancing_info_buffer;
	btr::BufferMemoryEx<mat4> m_world_buffer;
	btr::BufferMemoryEx<cModel::ModelInfo> m_model_info_buffer;
	btr::BufferMemoryEx<NodeInfo> m_node_info_buffer;
	btr::BufferMemoryEx<BoneInfo> m_bone_info_buffer;
	btr::BufferMemoryEx<AnimationInfo> m_animationinfo_buffer;
	btr::BufferMemoryEx<PlayingAnimation> m_animationplay_buffer;
	btr::BufferMemoryEx<mat4> m_node_transform_buffer;
	btr::BufferMemoryEx<mat4> m_bone_transform_buffer;
	btr::BufferMemoryEx<u32> m_instance_map_buffer;
	btr::BufferMemoryEx<cModel::Mesh> m_draw_indirect_buffer;

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
};

struct AppModelAnimateDescriptor : public DescriptorModuleOld
{
	struct Set
	{
		TypedBufferInfo<cModel::ModelInfo> m_model_info;
		TypedBufferInfo<ModelInstancingInfo> m_instancing_info;
		TypedBufferInfo<AnimationInfo> m_animation_info;
		TypedBufferInfo<PlayingAnimation> m_playing_animation;
		TypedBufferInfo<NodeInfo> m_node_info;
		TypedBufferInfo<BoneInfo> m_bone_info;
		TypedBufferInfo<mat4> m_world;
		TypedBufferInfo<mat4> m_node_transform;
		TypedBufferInfo<mat4> m_bone_transform;
		TypedBufferInfo<uint32_t> m_instance_map;
		TypedBufferInfo<uvec3> m_anime_indirect;
		TypedBufferInfo<cModel::Mesh> m_draw_indirect;

		std::array<vk::DescriptorImageInfo, 1> m_motion_texture;
	};

	AppModelAnimateDescriptor(const std::shared_ptr<btr::Context>& context)
	{
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(5),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(6),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(7),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(8),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(9),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(10),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setBinding(32),
		};
		m_descriptor_set_layout = createDescriptorSetLayout(context, binding);
		m_descriptor_pool = createDescriptorPool(context, binding, 30);
		m_context = context;
	}

	DescriptorSet<Set> allocateDescriptorSet(Set&& set)
	{
		DescriptorSet<Set> desc;
		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(m_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		desc.m_handle = std::move(m_context->m_device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);

		vk::DescriptorBufferInfo storages[] = {
			set.m_model_info,
			set.m_instancing_info,
			set.m_animation_info,
			set.m_playing_animation,
			set.m_node_info,
			set.m_bone_info,
			set.m_node_transform,
			set.m_world,
			set.m_instance_map,
			set.m_draw_indirect,
			set.m_bone_transform,
		};

		std::vector<vk::DescriptorImageInfo> images =
		{
			set.m_motion_texture[0]
		};

		std::vector<vk::WriteDescriptorSet> write =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(desc.m_handle.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(images.size())
			.setPImageInfo(images.data())
			.setDstBinding(32)
			.setDstSet(desc.m_handle.get()),
		};
		m_context->m_device->updateDescriptorSets(write, {});
		desc.m_descriptors = set;
		return desc;
	}

	std::shared_ptr<btr::Context> m_context;
};

struct AppModelRenderDescriptor : public DescriptorModuleOld
{
	enum {
		DESCRIPTOR_TEXTURE_NUM = 16,
	};

	struct Set
	{
		TypedBufferInfo<cModel::ModelInfo> m_model_info;
		TypedBufferInfo<ModelInstancingInfo> m_instancing_info;
		TypedBufferInfo<mat4> m_bonetransform;
		TypedBufferInfo<uint32_t> m_material_index;
		TypedBufferInfo<MaterialBuffer> m_material;
		std::array<vk::DescriptorImageInfo, DESCRIPTOR_TEXTURE_NUM> m_images;
	};

	AppModelRenderDescriptor(const std::shared_ptr<btr::Context>& context)
	{
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorCount(DESCRIPTOR_TEXTURE_NUM)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setBinding(5),
		};
		m_descriptor_set_layout = createDescriptorSetLayout(context, binding);
		m_descriptor_pool = createDescriptorPool(context, binding, 30);
		m_context = context;
	}

	DescriptorSet<Set> allocateDescriptorSet(Set&& set)
	{
		DescriptorSet<Set> desc;
		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(m_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		desc.m_handle = std::move(m_context->m_device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
		vk::DescriptorBufferInfo storages[] = {
			set.m_model_info,
			set.m_instancing_info,
			set.m_bonetransform,
			set.m_material_index,
			set.m_material,
		};

		vk::WriteDescriptorSet write[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(desc.m_handle.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount((uint32_t)set.m_images.size())
			.setPImageInfo(set.m_images.data())
			.setDstBinding(5)
			.setDstSet(desc.m_handle.get()),
		};
		m_context->m_device->updateDescriptorSets(array_length(write), write, 0, 0);

		desc.m_descriptors = set;
		return desc;
	}
	std::shared_ptr<btr::Context> m_context;
};

using sModelAnimateDescriptor = SingletonEx<AppModelAnimateDescriptor>;
using sModelRenderDescriptor = SingletonEx<AppModelRenderDescriptor>;
extern DescriptorSet<AppModelAnimateDescriptor::Set> createAnimateDescriptorSet(const std::shared_ptr<AppModel>& appModel);
extern DescriptorSet<AppModelRenderDescriptor::Set> createRenderDescriptorSet(const std::shared_ptr<AppModel>& appModel);
