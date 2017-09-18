#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCmdPool.h>
#include <btrlib/Loader.h>
#include <btrlib/sGlobal.h>
namespace app
{


struct App
{
	cGPU m_gpu;
	std::shared_ptr<cCmdPool> m_cmd_pool;
	std::shared_ptr<cWindow> m_window; // !< mainwindow
	std::vector<std::shared_ptr<cWindow>> m_window_list;
	std::shared_ptr<btr::Loader> m_loader;
	std::shared_ptr<btr::Executer> m_executer;

	std::vector<vk::CommandBuffer> m_system_cmds;
	SynchronizedPoint m_sync_point;
	App();
	void setup(const cGPU& gpu);
	void submit(std::vector<vk::CommandBuffer>&& cmds);
	void preUpdate();
	void postUpdate();
};


glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size);

}

