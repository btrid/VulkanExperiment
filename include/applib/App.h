#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/Loader.h>
namespace app
{

struct App
{
	cGPU m_gpu;
	std::shared_ptr<cWindow> m_window;
	std::shared_ptr<btr::Loader> m_loader;

	App();
	void setup(std::shared_ptr<btr::Loader>& loader);
};


glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size);

}

