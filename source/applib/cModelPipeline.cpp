
#include <applib/cModelPipeline.h>
#include <applib/sCameraManager.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <applib/DrawHelper.h>

struct DefaultAnimationModule : public AnimationModule
{
	DefaultAnimationModule(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource)
	{
		m_model_resource = resource;
		// bone memory allocate
		{
			btr::UpdateBufferExDescriptor desc;
			desc.alloc_size = (uint32_t)(resource->mBone.size() * sizeof(glm::mat4));
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.frame_max = sGlobal::FRAME_MAX;
			m_bone_buffer_staging.resize(sGlobal::FRAME_MAX);
			for (auto& buffer : m_bone_buffer_staging)
			{
				btr::AllocatedMemory::Descriptor desc;
				desc.size = resource->mBone.size() * sizeof(glm::mat4);
				buffer = context->m_staging_memory.allocateMemory(desc);
			}
			{
				btr::AllocatedMemory::Descriptor desc;
				desc.size = resource->mBone.size() * sizeof(glm::mat4);
				m_bone_buffer = context->m_storage_memory.allocateMemory(desc);
			}
		}

		// recode command
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			m_bone_update_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);
			for (size_t i = 0; i < m_bone_update_cmd.size(); i++)
			{
				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
				vk::CommandBufferInheritanceInfo inheritance_info;
				begin_info.setPInheritanceInfo(&inheritance_info);

				auto& cmd = m_bone_update_cmd[i].get();
				cmd.begin(begin_info);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, m_bone_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite), {});

				vk::BufferCopy copy;
				copy.dstOffset = m_bone_buffer.getBufferInfo().offset;
				copy.size = m_bone_buffer.getBufferInfo().range;
				copy.srcOffset = m_bone_buffer_staging[i].getBufferInfo().offset;
				cmd.copyBuffer(m_bone_buffer_staging[i].getBufferInfo().buffer, m_bone_buffer.getBufferInfo().buffer, copy);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, m_bone_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead), {});
				cmd.end();
			}

		}

	}

	std::shared_ptr<cModel::Resource> m_model_resource;

	btr::BufferMemory m_bone_buffer;
	std::vector<btr::BufferMemory> m_bone_buffer_staging;

	std::vector<vk::UniqueCommandBuffer> m_bone_update_cmd;

	virtual btr::BufferMemory getBoneBuffer()const override { return m_bone_buffer; }
	virtual void update() override
	{
		m_playlist.execute();
		std::vector<glm::mat4> node_buffer(m_model_resource->m_model_info.mNodeNum);

		updateNodeTransform(0, m_model_transform.calcGlobal()*m_model_transform.calcLocal(), node_buffer);

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
	virtual void execute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd) override
	{
		cmd.executeCommands(m_bone_update_cmd[context->getGPUFrame()].get());
	}

private:
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
					if (nodeAnim->m_translate[pos_index + 1].m_time >= target_time) {
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

};


struct DefaultModelPipelineComponent : public ModelPipelineComponent
{
	enum {
		DESCRIPTOR_TEXTURE_NUM = 16,
	};

	DefaultModelPipelineComponent(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderPassModule>& render_pass, const std::shared_ptr<ShaderModule>& shader)
	{
		auto& device = context->m_device;
		m_render_pass = render_pass;
		m_shader = shader;

		// Create descriptor set
		{
			std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
			bindings[DESCRIPTOR_SET_LAYOUT_MODEL] =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(2),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(3),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(4),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(DESCRIPTOR_TEXTURE_NUM)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setBinding(5),
			};

			for (u32 i = 0; i < bindings.size(); i++)
			{
				vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
					.setBindingCount(bindings[i].size())
					.setPBindings(bindings[i].data());
				m_descriptor_set_layout[i] = device->createDescriptorSetLayoutUnique(descriptor_layout_info);
			}
			// DescriptorPool
			{
				m_model_descriptor_pool = createDescriptorPool(device.getHandle(), bindings, 30);
			}
		}


		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_MODEL].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA)
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			m_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info);

		}

		// pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			vk::Extent3D size;
			size.setWidth(context->m_window->getClientSize().x);
			size.setHeight(context->m_window->getClientSize().y);
			size.setDepth(1);
			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height));
			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(1);
			viewport_info.setPViewports(&viewport);
			viewport_info.setScissorCount(1);
			viewport_info.setPScissors(&scissor);

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eBack);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setLineWidth(1.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			// ブレンド
			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			auto vertex_input_binding = cModel::GetVertexInputBinding();
			auto vertex_input_attribute = cModel::GetVertexInputAttribute();
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)shader->getShaderStageInfo().size())
				.setPStages(shader->getShaderStageInfo().data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout.get())
				.setRenderPass(render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}

	std::shared_ptr<ModelRender> createRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<Model>& model) override
	{
		auto& device = context->m_device;
		auto render = std::make_shared<ModelRender>();

		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_MODEL].get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(m_model_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		render->m_descriptor_set_model = std::move(device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);

		std::vector<vk::DescriptorBufferInfo> storages = {
			model->m_animation->getBoneBuffer().getBufferInfo(),
			model->m_material->getMaterialIndexBuffer().getBufferInfo(),
			model->m_material->getMaterialBuffer().getBufferInfo(),
		};

		std::vector<vk::DescriptorImageInfo> color_images(DESCRIPTOR_TEXTURE_NUM, vk::DescriptorImageInfo(DrawHelper::Order().getWhiteTexture().m_sampler.get(), DrawHelper::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		for (size_t i = 0; i < model->m_model_resource->m_mesh.size(); i++)
		{
			auto& material = model->m_model_resource->m_material[model->m_model_resource->m_mesh[i].m_material_index];
			color_images[i] = vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		}
		std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount((uint32_t)storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(2)
			.setDstSet(render->m_descriptor_set_model.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount((uint32_t)color_images.size())
			.setPImageInfo(color_images.data())
			.setDstBinding(5)
			.setDstSet(render->m_descriptor_set_model.get()),
		};
		device->updateDescriptorSets(drawWriteDescriptorSets, {});

		// recode command
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			render->m_draw_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);

			for (size_t i = 0; i < render->m_draw_cmd.size(); i++)
			{
				auto& cmd = render->m_draw_cmd[i].get();
				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
				vk::CommandBufferInheritanceInfo inheritance_info;
				inheritance_info.setFramebuffer(getRenderPassModule()->getFramebuffer(i));
				inheritance_info.setRenderPass(getRenderPassModule()->getRenderPass());
				begin_info.pInheritanceInfo = &inheritance_info;

				cmd.begin(begin_info);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, getPipeline());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, render->m_descriptor_set_model.get(), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
				cmd.bindVertexBuffers(0, { model->m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { model->m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
				cmd.bindIndexBuffer(model->m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, model->m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, model->m_model_resource->m_mesh_resource.mIndexType);
				cmd.drawIndexedIndirect(model->m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, model->m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, model->m_model_resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));

				cmd.end();
			}
		}

		return render;
	}
	virtual const std::shared_ptr<RenderPassModule>& getRenderPassModule()const override { return m_render_pass; }
	virtual vk::Pipeline getPipeline()const override { return m_pipeline.get(); }

private:

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MODEL,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};

	vk::UniquePipeline m_pipeline;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniqueDescriptorPool m_model_descriptor_pool;

	std::shared_ptr<RenderPassModule> m_render_pass;
	std::shared_ptr<ShaderModule> m_shader;

};



void cModelPipeline::setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<ModelPipelineComponent>& pipeline /*= nullptr*/)
{
	m_pipeline = pipeline;
	if (!m_pipeline)
	{
		// なかったらデフォルトで初期化
		auto render_pass = std::make_shared<RenderPassModule>(context);
		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		std::vector<ShaderDescriptor> shader_desc =
		{
			{ path + "ModelRender.vert.spv",vk::ShaderStageFlagBits::eVertex },
			{ path + "ModelRender.frag.spv",vk::ShaderStageFlagBits::eFragment },
		};
		auto shader = std::make_shared<ShaderModule>(context, shader_desc);
		m_pipeline = std::make_shared<DefaultModelPipelineComponent>(context, render_pass, shader);
	}
}

vk::CommandBuffer cModelPipeline::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	// draw
	for (auto& render : m_model)
	{
		render->m_animation->execute(context, cmd);
	}

	vk::RenderPassBeginInfo begin_render_Info;
	begin_render_Info.setRenderPass(m_pipeline->getRenderPassModule()->getRenderPass());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()));
	begin_render_Info.setFramebuffer(m_pipeline->getRenderPassModule()->getFramebuffer(context->getGPUFrame()));
	cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eSecondaryCommandBuffers);

	for (auto& render : m_model)
	{
		render->m_render->draw(context, cmd);
	}
	cmd.endRenderPass();

	cmd.end();
	return cmd;
}
std::shared_ptr<Model> cModelPipeline::createRender(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource)
{
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	auto model = std::make_shared<Model>();
	model->m_model_resource = resource;
	model->m_material = std::make_shared<DefaultMaterialModule>(context, resource);
	model->m_animation = std::make_shared<DefaultAnimationModule>(context, resource);
	model->m_render = m_pipeline->createRender(context, model);
	

	m_model.push_back(model);
	return model;

}
