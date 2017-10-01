
#include <applib/cModelInstancingPipeline.h>
#include <applib/cModelInstancingRender.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <btrlib/cModel.h>
#include <applib/sCameraManager.h>

void cModelInstancingPipeline::addModel(std::shared_ptr<ModelInstancingModel>& model)
{
	m_model.emplace_back(model);
}

vk::CommandBuffer cModelInstancingPipeline::execute(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	for (auto& render : m_model)
	{
		render->execute(context, cmd);
	}
	cmd.end();
	return cmd;
}

vk::CommandBuffer cModelInstancingPipeline::draw(std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

	vk::RenderPassBeginInfo render_begin_info;
	render_begin_info.setRenderPass(m_render_pipeline->m_render_pass->getRenderPass());
	render_begin_info.setFramebuffer(m_render_pipeline->m_render_pass->getFramebuffer(context->getGPUFrame()));
	render_begin_info.setRenderArea(vk::Rect2D({}, context->m_window->getClientSize<vk::Extent2D>()));

	cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);

	// draw
	for (auto& render : m_model)
	{
		render->draw(context, cmd);
	}

	cmd.endRenderPass();
	cmd.end();
	return cmd;
}

std::shared_ptr<ModelInstancingModel> cModelInstancingPipeline::createModel(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource)
{
	auto model = std::make_shared<ModelInstancingModel>();
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	model->m_material = std::make_shared<DefaultMaterialModule>(context, resource);

	auto instanceNum = 1000;
	model->m_instancing = m_execute_pipeline->createRender(context, resource, instanceNum);
	model->m_render = m_render_pipeline->createRender(context, resource, instanceNum, model);
	return model;

}
