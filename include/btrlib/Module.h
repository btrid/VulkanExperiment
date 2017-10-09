#pragma once

struct Component
{
	virtual ~Component() = default;
};

struct RenderComponent : public Component {};
struct Drawable : public Component {};

struct RenderPassModule
{
	RenderPassModule(const std::shared_ptr<btr::Context>& context)
	{
		auto& device = context->m_device;
		// �����_�[�p�X
		{
			// sub pass
			std::vector<vk::AttachmentReference> color_ref =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};
			vk::AttachmentReference depth_ref;
			depth_ref.setAttachment(1);
			depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount((uint32_t)color_ref.size());
			subpass.setPColorAttachments(color_ref.data());
			subpass.setPDepthStencilAttachment(&depth_ref);

			std::vector<vk::AttachmentDescription> attach_description = {
				// color1
				vk::AttachmentDescription()
				.setFormat(context->m_window->getSwapchain().m_surface_format.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				vk::AttachmentDescription()
				.setFormat(context->m_window->getSwapchain().m_depth.m_format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount((uint32_t)attach_description.size())
				.setPAttachments(attach_description.data())
				.setSubpassCount(1)
				.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}

		m_framebuffer.resize(context->m_window->getSwapchain().getBackbufferNum());
		{
			std::array<vk::ImageView, 2> view;

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount((uint32_t)view.size());
			framebuffer_info.setPAttachments(view.data());
			framebuffer_info.setWidth(context->m_window->getClientSize().x);
			framebuffer_info.setHeight(context->m_window->getClientSize().y);
			framebuffer_info.setLayers(1);

			for (size_t i = 0; i < m_framebuffer.size(); i++) {
				view[0] = context->m_window->getSwapchain().m_backbuffer[i].m_view;
				view[1] = context->m_window->getSwapchain().m_depth.m_view;
				m_framebuffer[i] = context->m_device->createFramebufferUnique(framebuffer_info);
			}
		}
	}

	virtual vk::RenderPass getRenderPass()const { return m_render_pass.get(); }
	virtual vk::Framebuffer getFramebuffer(uint32_t index)const { return m_framebuffer[index].get(); }

private:
	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;

};

struct ShaderDescriptor
{
	std::string filepath;
	vk::ShaderStageFlagBits stage;
};
struct ShaderModule
{
	ShaderModule(const std::shared_ptr<btr::Context>& context, const std::vector<ShaderDescriptor>& desc)
	{
		auto& device = context->m_device;
		// setup shader
		{
			m_shader_list.resize(desc.size());
			m_stage_info.resize(desc.size());
			for (size_t i = 0; i < desc.size(); i++) {
				m_shader_list[i] = std::move(loadShaderUnique(device.getHandle(), desc[i].filepath));
				m_stage_info[i].setModule(m_shader_list[i].get());
				m_stage_info[i].setStage(desc[i].stage);
				m_stage_info[i].setPName("main");
			}
		}
	}
	const std::vector<vk::PipelineShaderStageCreateInfo>& getShaderStageInfo()const { return m_stage_info; }

private:
	std::vector<vk::UniqueShaderModule> m_shader_list;
	std::vector<vk::PipelineShaderStageCreateInfo> m_stage_info;
};

struct PipelineComponent
{
};

struct InstancingModule
{
	virtual vk::DescriptorBufferInfo getModelInfo()const = 0;
	virtual vk::DescriptorBufferInfo getInstancingInfo()const = 0;
};

struct UniqueDescriptorModule
{
protected:
	void createDescriptor(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
		descriptor_layout_info.setBindingCount((uint32_t)binding.size());
		descriptor_layout_info.setPBindings(binding.data());
		m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(descriptor_layout_info);

		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(context->m_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
	}

protected:
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;
public:
	vk::DescriptorSetLayout getLayout()const { return m_descriptor_set_layout.get(); }
	vk::DescriptorSet getSet()const { return m_descriptor_set.get(); }

};

struct DescriptorModule
{
public:
	vk::UniqueDescriptorSet allocateDescriptorSet(const std::shared_ptr<btr::Context>& context)
	{
		auto& device = context->m_device;
		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(getPool());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		auto descriptor_set = std::move(device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
		return descriptor_set;
	}
protected:
	vk::UniqueDescriptorSetLayout createDescriptorSetLayout(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
		descriptor_layout_info.setBindingCount((uint32_t)binding.size());
		descriptor_layout_info.setPBindings(binding.data());
		return context->m_device->createDescriptorSetLayoutUnique(descriptor_layout_info);
	}
	vk::UniqueDescriptorPool createDescriptorPool(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding, uint32_t set_size)
	{
		auto& device = context->m_device;
		std::vector<vk::DescriptorPoolSize> pool_size(VK_DESCRIPTOR_TYPE_RANGE_SIZE);
		for (auto& b : binding)
		{
			pool_size[(uint32_t)b.descriptorType].setType(b.descriptorType);
			pool_size[(uint32_t)b.descriptorType].descriptorCount += b.descriptorCount*set_size;
		}
		pool_size.erase(std::remove_if(pool_size.begin(), pool_size.end(), [](auto& p) {return p.descriptorCount == 0; }), pool_size.end());
		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setPoolSizeCount((uint32_t)pool_size.size());
		pool_info.setPPoolSizes(pool_size.data());
		pool_info.setMaxSets(set_size);
		pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
		return device->createDescriptorPoolUnique(pool_info);
	}

protected:
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorPool m_descriptor_pool;
public:
	vk::DescriptorSetLayout getLayout()const { return m_descriptor_set_layout.get(); }
protected:
	vk::DescriptorPool getPool()const { return m_descriptor_pool.get(); }

};