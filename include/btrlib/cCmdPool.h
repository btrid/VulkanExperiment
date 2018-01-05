#pragma once

#include <array>
#include <vector>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/sGlobal.h>

namespace btr
{
struct Context;
}

struct cCmdPool;
struct cCmdPool
{
	cCmdPool(const std::shared_ptr<btr::Context>& context);

	enum CmdPoolType
	{
		CMD_POOL_TYPE_ONETIME,	//!< 1フレームに一度poolがリセットされる 
		CMD_POOL_TYPE_COMPILED,		//!< cmdは自分で管理する
	};

	struct CmdPoolPerFamily
	{
		std::array<vk::UniqueCommandPool, sGlobal::FRAME_MAX> m_cmd_pool_onetime;
		std::array<std::vector<vk::UniqueCommandBuffer>, sGlobal::FRAME_MAX> m_cmd_onetime_deleter;

		vk::UniqueCommandPool	m_cmd_pool_compiled;

	};
	struct CmdPoolPerThread
	{
		std::vector<CmdPoolPerFamily>	m_per_family;
	};

	std::vector<CmdPoolPerThread> m_per_thread;

	std::array<std::vector<vk::CommandBuffer>, sGlobal::FRAME_MAX> m_cmds;

	std::shared_ptr<btr::Context> m_context;

	vk::CommandBuffer allocCmdOnetime(int device_family_index);
	vk::CommandBuffer allocCmdTempolary(uint32_t device_family_index);
	vk::CommandPool getCmdPool(CmdPoolType type, int device_family_index)const;
	vk::CommandBuffer get();

	std::array<std::vector<vk::CommandBuffer>, sGlobal::FRAME_MAX>& getCmds() { return m_cmds; }

	void resetPool();
	std::vector<vk::CommandBuffer> submit();
};
