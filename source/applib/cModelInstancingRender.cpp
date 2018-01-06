#include <applib/cModelInstancingRender.h>
#include <applib/cModelInstancingPipeline.h>
#include <applib/cModelPipeline.h>
#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>

void ModelInstancingModule::addModel(const InstanceResource* data, uint32_t num)
{
	auto frame = sGlobal::Order().getWorkerFrame();
	auto index = m_instance_count[frame].fetch_add(num);
	auto* staging = m_world_buffer.mapSubBuffer(frame, index);
	for (uint32_t i = 0; i < num; i++)
	{
		staging[i] = data[i].m_world;
	}

}

void ModelInstancingModel::execute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd)
{
	m_instancing->animationExecute(context, cmd);
}

void ModelInstancingModel::draw(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd)
{
	cmd.executeCommands(m_render->m_draw_cmd[context->getGPUFrame()].get());
}
