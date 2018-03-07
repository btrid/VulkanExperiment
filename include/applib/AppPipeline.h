#pragma once

#include <memory>
#include <bitset>
#include <mutex>
#include <functional>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCmdPool.h>
#include <btrlib/Context.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Module.h>
#include <applib/App.h>

struct IPipeline
{
	virtual vk::CommandBuffer execute() = 0;
};
struct ClearPipeline : IPipeline
{
	ClearPipeline(btr::Context* context, const RenderTarget& render_target);

	vk::CommandBuffer execute() override
	{
		return m_cmd.get();
	}

	vk::UniqueCommandBuffer m_cmd;
};

struct PresentPipeline : IPipeline
{
	PresentPipeline(const std::shared_ptr<btr::Context>& context, const RenderTarget& render_target, const std::shared_ptr<Swapchain>& swapchain);

	vk::CommandBuffer execute() override
	{
		return m_cmd[m_swapchain->m_backbuffer_index].get();
	}
	std::vector<vk::UniqueImageView> m_backbuffer_view;

	vk::UniqueDescriptorSetLayout m_descriptor_layout;
	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;
	vk::UniqueShaderModule m_shader[2];

	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;

	std::shared_ptr<Swapchain> m_swapchain;
	vk::UniqueDescriptorSet m_descriptor_set;
	std::vector<vk::UniqueCommandBuffer> m_cmd;

};
