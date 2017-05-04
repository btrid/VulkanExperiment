#pragma once
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
struct App
{
	cWindow m_window;
	vk::RenderPass m_render_pass;
	std::vector<vk::Framebuffer> m_framebuffer;
	App();
};