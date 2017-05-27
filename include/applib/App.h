#pragma once
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
namespace app
{

struct App
{
	cWindow m_window;
	vk::RenderPass m_render_pass;
	std::vector<vk::Framebuffer> m_framebuffer;
	App();
};

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size);

}

