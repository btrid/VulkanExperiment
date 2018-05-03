
#include <applib/AppModel/AppModelPipeline.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <btrlib/cModel.h>
#include <applib/sCameraManager.h>

// vk::CommandBuffer cModelInstancingPipeline::draw(std::shared_ptr<btr::Context>& context)
// {
// 	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
// 
// 	vk::RenderPassBeginInfo render_begin_info;
// 	render_begin_info.setRenderPass(m_render_pipeline->m_render_pass->getRenderPass());
// 	render_begin_info.setFramebuffer(m_render_pipeline->m_render_pass->getFramebuffer(context->getGPUFrame()));
// 	render_begin_info.setRenderArea(vk::Rect2D({}, context->m_window->getClientSize<vk::Extent2D>()));
// 
// 	cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);
// 
// 	// draw
// 	for (auto& render : m_model)
// 	{
// 		render->draw(context, cmd);
// 	}
// 
// 	cmd.endRenderPass();
// 	cmd.end();
// 	return cmd;
// }
