#include <003_particle/Collision.h>

void sCollisionSystem::setup(std::shared_ptr<btr::Loader>& loader)
{
	{
		struct ShaderDesc {
			const char* name;
			vk::ShaderStageFlagBits stage;
		} shader_desc[] =
		{
			{ "CollisionTest.comp.spv", vk::ShaderStageFlagBits::eCompute },
		};
		static_assert(array_length(shader_desc) == SHADER_NUM, "not equal shader num");
		std::string path = btr::getResourceAppPath() + "shader\\binary\\";
		for (uint32_t i = 0; i < SHADER_NUM; i++)
		{
			m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_desc[i].name));
			m_shader_info[i].setStage(shader_desc[i].stage);
			m_shader_info[i].setPName("main");
		}
	}

}

void sCollisionSystem::execute(std::shared_ptr<btr::Executer>& executer)
{

}
