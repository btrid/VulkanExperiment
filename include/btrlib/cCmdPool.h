#pragma once

#include <array>
#include <vector>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>

namespace btr
{
struct Context;
}

struct cCmdPool;
struct cCmdPool
{
	cCmdPool(const std::shared_ptr<btr::Context>& context);

	std::vector<vk::UniqueCommandPool>	m_cmd_pool_system;

	struct Cmd
	{
		vk::UniqueCommandPool m_cmd_pool;
		std::vector<vk::UniqueCommandBuffer> m_cmd_onetime_deleter;
	};
	std::vector<std::array<Cmd, sGlobal::FRAME_COUNT_MAX>> m_cmd;
	std::vector<vk::CommandBuffer> m_tls_cmds;

	std::shared_ptr<btr::Context> m_context;

	vk::CommandBuffer allocCmdImpl();
	vk::CommandBuffer allocCmdOnetime(int device_family_index, const char* name = nullptr);
	vk::CommandBuffer allocCmdTempolary(uint32_t device_family_index, const char* name = nullptr);

	void resetPool();
	std::vector<vk::CommandBuffer> submit();

	// 以下廃止予定
	enum CmdPoolType
	{
		CMD_POOL_TYPE_ONETIME,	//!< 1フレームに一度poolがリセットされる 
		CMD_POOL_TYPE_COMPILED,		//!< cmdは自分で管理する
	};
	vk::CommandPool getCmdPool(CmdPoolType type, int device_family_index) const;

};
