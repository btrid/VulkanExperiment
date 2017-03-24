#include <002_model/cModelRender.h>
#include <002_model/cModelRenderer.h>

void ModelRender::setup(cModelRenderer&  renderer)
{
	// setup graphics cmdbuffer 
	{
		cDevice graphics_device = sThreadData::Order().m_device[sThreadData::DEVICE_GRAPHICS];
		{
			auto& graphics_cmd_pool = sThreadData::Order().getCmdPoolCompiled(graphics_device.getQueueFamilyIndex());
			vk::CommandBufferAllocateInfo cmd_alloc_info;
			cmd_alloc_info.setCommandBufferCount(1);
			cmd_alloc_info.setCommandPool(graphics_cmd_pool[0]);
			cmd_alloc_info.setLevel(vk::CommandBufferLevel::ePrimary);
			m_cmd_graphics = graphics_device->allocateCommandBuffers(cmd_alloc_info);
		}

		auto pipeline = renderer.getDrawPipeline();

		// update desctiptor
		{
			// モデルごとのDescriptorの設定
			vk::DescriptorSetAllocateInfo alloc_info = vk::DescriptorSetAllocateInfo()
				.setDescriptorPool(pipeline.m_descriptor_pool)
				.setDescriptorSetCount(1)
				.setPSetLayouts(&pipeline.m_descriptor_set_layout[cModelRenderer::cModelDrawPipeline::DESCRIPTOR_SET_LAYOUT_PER_MODEL]);
			m_draw_descriptor_set_per_model = graphics_device->allocateDescriptorSets(alloc_info)[0];

			std::vector<vk::DescriptorBufferInfo> uniformBufferInfo = {
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).mBufferInfo,
			};

			std::vector<vk::DescriptorBufferInfo> storagemBufferInfo = {
				mPrivate->getBuffer(Private::ModelBuffer::VS_MATERIAL).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).mBufferInfo,
			};
			std::vector<vk::DescriptorBufferInfo> fStoragemBufferInfo = {
				mPrivate->getBuffer(Private::ModelBuffer::MATERIAL).mBufferInfo,
			};
			std::vector<vk::DescriptorImageInfo> color_image_info = {
				vk::DescriptorImageInfo(mPrivate->m_material[0].mDiffuseTex.m_sampler, mPrivate->m_material[0].mDiffuseTex.m_image_view, vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			std::vector<vk::WriteDescriptorSet> write_descriptor_set =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniformBufferInfo.size())
				.setPBufferInfo(uniformBufferInfo.data())
				.setDstBinding(0)
				.setDstSet(m_draw_descriptor_set_per_model),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storagemBufferInfo.size())
				.setPBufferInfo(storagemBufferInfo.data())
				.setDstBinding(1)
				.setDstSet(m_draw_descriptor_set_per_model),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(fStoragemBufferInfo.size())
				.setPBufferInfo(fStoragemBufferInfo.data())
				.setDstBinding(16)
				.setDstSet(m_draw_descriptor_set_per_model),
			};

			graphics_device->updateDescriptorSets(write_descriptor_set, {});
		}
		{
			// meshごとの更新
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = pipeline.m_descriptor_pool;
			allocInfo.descriptorSetCount = mPrivate->mMeshNum;
			allocInfo.pSetLayouts = &pipeline.m_descriptor_set_layout[cModelRenderer::cModelDrawPipeline::DESCRIPTOR_SET_LAYOUT_PER_MESH];
			m_draw_descriptor_set_per_mesh = graphics_device->allocateDescriptorSets(allocInfo);
			for (size_t i = 0; i < m_draw_descriptor_set_per_mesh.size(); i++)
			{
				auto& material = mPrivate->m_material[mPrivate->m_material_index[i]];
				material.mDiffuseTex.m_image_view;

				std::vector<vk::DescriptorImageInfo> color_image_info = {
					vk::DescriptorImageInfo(material.mDiffuseTex.m_sampler, material.mDiffuseTex.m_image_view, vk::ImageLayout::eShaderReadOnlyOptimal),
				};
				std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(color_image_info.size())
					.setPImageInfo(color_image_info.data())
					.setDstBinding(32)
					.setDstSet(m_draw_descriptor_set_per_mesh[i]),
				};
				graphics_device->updateDescriptorSets(drawWriteDescriptorSets, {});
			}

		}


	}

	// todo compute
	return;
	//vk::CommandBuffer cModel::buildExecuteCmd()
	{
		cDevice compute_device = sThreadData::Order().m_device[sThreadData::DEVICE_COMPUTE];
		{
			auto& compute_cmd_pool = sThreadData::Order().getCmdPoolCompiled(compute_device.getQueueFamilyIndex());
			vk::CommandBufferAllocateInfo cmd_alloc_info;
			cmd_alloc_info.setCommandBufferCount(1);
			cmd_alloc_info.setCommandPool(compute_cmd_pool[0]);
			cmd_alloc_info.setLevel(vk::CommandBufferLevel::ePrimary);
			m_cmd_compute = compute_device->allocateCommandBuffers(cmd_alloc_info);
		}

		auto pipeline = renderer.getDrawPipeline();
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.setDescriptorPool(pipeline.m_descriptor_pool);
		allocInfo.setDescriptorSetCount(7);
		allocInfo.setPSetLayouts(&pipeline.m_descriptor_set_layout[0]);
		m_compute_descriptor_set = compute_device->allocateDescriptorSets(allocInfo);

		std::vector<vk::WriteDescriptorSet> compute_write_descriptor;
		vk::WriteDescriptorSet desc;
		// Clear
		{
			std::vector<vk::DescriptorBufferInfo> storages =
			{
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).mBufferInfo,
				mPrivate->mMesh.mIndirectInfo,
			};

			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[0]);
			compute_write_descriptor.emplace_back(desc);

		}
		// AnimationUpdate
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				mPrivate->getMotionBuffer(cAnimation::ANIMATION_INFO).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::PLAYING_ANIMATION).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[1]);
			compute_write_descriptor.emplace_back(desc);

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).mBufferInfo,
				mPrivate->mMesh.mIndirectInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[1]);
			compute_write_descriptor.emplace_back(desc);
		}
		// MotionUpdate
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[2]);
			compute_write_descriptor.emplace_back(desc);

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				mPrivate->getMotionBuffer(cAnimation::ANIMATION_INFO).mBufferInfo,
				mPrivate->getMotionBuffer(cAnimation::MOTION_INFO).mBufferInfo,
				mPrivate->getMotionBuffer(cAnimation::MOTION_DATA_TIME).mBufferInfo,
				mPrivate->getMotionBuffer(cAnimation::MOTION_DATA_SRT).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::PLAYING_ANIMATION).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::NODE_LOCAL_TRANSFORM).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::MOTION_WORK).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[2]);
			compute_write_descriptor.emplace_back(desc);
		}

		// NodeTransform
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[3]);
			compute_write_descriptor.emplace_back(desc);

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				mPrivate->getBuffer(Private::ModelBuffer::WORLD).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::NODE_LOCAL_TRANSFORM).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::NODE_GLOBAL_TRANSFORM).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[3]);
			compute_write_descriptor.emplace_back(desc);
		}

		// CameraCulling			
		{
			std::vector<vk::DescriptorBufferInfo> uniforms = {};
			std::vector<vk::DescriptorBufferInfo> storages =
			{
				mPrivate->getBuffer(Private::ModelBuffer::WORLD).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::NODE_INFO).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::BONE_INFO).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::BONE_MAP).mBufferInfo,
				mPrivate->mMesh.mIndirectInfo,
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[4]);
			compute_write_descriptor.emplace_back(desc);
		}

		// BoneTransform
		{
			std::vector<vk::DescriptorBufferInfo> uniforms =
			{
				mPrivate->getBuffer(Private::ModelBuffer::MODEL_INFO).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_compute_descriptor_set[5]);
			compute_write_descriptor.emplace_back(desc);

			std::vector<vk::DescriptorBufferInfo> storages =
			{
				mPrivate->getBuffer(Private::ModelBuffer::NODE_GLOBAL_TRANSFORM).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::BONE_INFO).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::BONE_MAP).mBufferInfo,
				mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).mBufferInfo,
			};
			desc = vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(8)
				.setDstSet(m_compute_descriptor_set[6]);
			compute_write_descriptor.emplace_back(desc);
		}
		compute_device->updateDescriptorSets(compute_write_descriptor, {});

// 		for (size_t i = 0; i < renderer.getComputePipeline().m_layout.size(); i++)
// 		{
// 			m_cmd_compute[1].bindPipeline(vk::PipelineBindPoint::eCompute, renderer.getComputePipeline().m_pipeline[i]);
// 			m_cmd_compute[1].bindDescriptorSets(vk::PipelineBindPoint::eCompute, renderer.getComputePipeline().m_layout[i], 0, std::vector<vk::DescriptorSet>{ m_compute_descriptor_set[i] }, {});
// 			m_cmd_compute[1].dispatch(256, 1, 1);
// 			// 					m_cmd[1].dispatchIndirect(mComputePipeline.mDisptachIndirectInfo.buffer(), vk::DeviceSize(sizeof(glm::ivec3)*i));
// 		}

		std::vector<vk::BufferMemoryBarrier> barrier =
		{
			vk::BufferMemoryBarrier()
			.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
			.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eMemoryRead)
			.setBuffer(mPrivate->mMesh.mIndirectInfo.buffer)
			.setSize(mPrivate->mMesh.mIndirectInfo.range)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED),
			vk::BufferMemoryBarrier()
			.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
			.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eMemoryRead)
			.setBuffer(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).mBuffer)
			.setSize(mPrivate->getBuffer(Private::ModelBuffer::BONE_TRANSFORM).mBufferInfo.range)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED),
		};

// 		m_cmd_compute[1].pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
// 			vk::DependencyFlags(), {}, barrier, {});
	}
}

void ModelRender::draw(cModelRenderer& renderer, vk::CommandBuffer& cmd)
{

	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 2, renderer.getDrawPipeline().m_draw_descriptor_set_per_scene, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 0, m_draw_descriptor_set_per_model, {});
	for (auto i : mPrivate->m_material_index)
	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer.getDrawPipeline().m_pipeline_layout, 1, m_draw_descriptor_set_per_mesh[i], {});
		cmd.bindVertexBuffers(0, { mPrivate->mMesh.mBuffer }, { vk::DeviceSize() });
		cmd.bindIndexBuffer(mPrivate->mMesh.mBuffer, vk::DeviceSize(mPrivate->mMesh.mBufferSize[0]), vk::IndexType::eUint32);
		cmd.drawIndexedIndirect(mPrivate->mMesh.mBufferIndirect, vk::DeviceSize(0), mPrivate->mMesh.mIndirectCount, sizeof(cModel::Mesh));
	}
}
