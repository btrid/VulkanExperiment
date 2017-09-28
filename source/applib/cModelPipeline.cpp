
#include <applib/cModelPipeline.h>
#include <applib/sCameraManager.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <applib/DrawHelper.h>

void cModelPipeline::setup(std::shared_ptr<btr::Context>& context)
{
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
