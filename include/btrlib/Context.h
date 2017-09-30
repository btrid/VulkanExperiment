#pragma once

#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCmdPool.h>

namespace btr
{

struct Context
{
	cGPU m_gpu;
	cDevice m_device;

	btr::AllocatedMemory m_vertex_memory;
	btr::AllocatedMemory m_uniform_memory;
	btr::AllocatedMemory m_storage_memory;
	btr::AllocatedMemory m_staging_memory;

	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniquePipelineCache m_cache;

	std::shared_ptr<cWindow> m_window;
	std::shared_ptr<cCmdPool> m_cmd_pool;

	uint32_t getCPUFrame()const { return (m_window->getSwapchain().m_backbuffer_index + 1) % m_window->getSwapchain().getBackbufferNum(); }
	uint32_t getGPUFrame()const { return m_window->getSwapchain().m_backbuffer_index; }
};

}

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
		// レンダーパス
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

struct MaterialModule
{
	virtual btr::BufferMemory getMaterialIndexBuffer()const = 0;
	virtual btr::BufferMemory getMaterialBuffer()const = 0;
};
