
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
	m_pipeline = std::make_shared<ModelPipelineComponent>(context, render_pass, shader);
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
	begin_render_Info.setRenderPass(m_pipeline->getRenderPassComponent()->getRenderPass());
	begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(640, 480)));
	begin_render_Info.setFramebuffer(m_pipeline->getRenderPassComponent()->getFramebuffer(context->getGPUFrame()));
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
	auto render = std::make_shared<ModelRender>();

	{
		m_pipeline->createDescriptorSet(context, model, render);
	}


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
			inheritance_info.setFramebuffer(m_pipeline->getRenderPassComponent()->getFramebuffer(i));
			inheritance_info.setRenderPass(m_pipeline->getRenderPassComponent()->getRenderPass());
			begin_info.pInheritanceInfo = &inheritance_info;

			cmd.begin(begin_info);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->getPipeline());
			m_pipeline->bindDescriptor(render, cmd);
			cmd.bindVertexBuffers(0, { resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
			cmd.bindIndexBuffer(resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, resource->m_mesh_resource.mIndexType);
			cmd.drawIndexedIndirect(resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));

			cmd.end();
		}
	}
	model->m_render = std::move(render);
	m_model.push_back(model);
	return model;

}
