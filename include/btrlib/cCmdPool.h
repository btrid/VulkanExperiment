#pragma once

#include <array>
#include <vector>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/sGlobal.h>

namespace btr
{
struct Executer;
}

template<typename T>
struct DoubleBuffer
{
	std::array<T, 2> m_data;
	T& getCPUThreadData() { return m_data[sGlobal::Order().getCPUIndex()]; }
	T& getGPUThreadData() { return m_data[sGlobal::Order().getGPUIndex()]; }
};
struct cCmdPool
{
	static std::shared_ptr<cCmdPool> MakeCmdPool(cGPU& gpu);

	enum CmdPoolType
	{
		CMD_POOL_TYPE_ONETIME,	//!< 1フレームに一度poolがリセットされる 
		CMD_POOL_TYPE_TEMPORARY,		//!< 数フレームに渡ってcmdが使われる。自分で管理する 
		CMD_POOL_TYPE_COMPILED,		//!< cmdは自分で管理する
	};

	struct CmdPoolPerFamily
	{
		std::array<vk::UniqueCommandPool, sGlobal::FRAME_MAX> m_cmd_pool_onetime;
		vk::UniqueCommandPool	m_cmd_pool_temporary;
		vk::UniqueCommandPool	m_cmd_pool_compiled;

		std::array<std::vector<vk::UniqueCommandBuffer>, sGlobal::FRAME_MAX> m_cmd_onetime_deleter;
		std::vector<vk::UniqueCommandBuffer> m_cmd_queue;
	};
	struct CmdPoolPerThread
	{
		std::vector<CmdPoolPerFamily>	m_per_family;
	};
	std::vector<CmdPoolPerThread> m_per_thread;

	vk::CommandBuffer getCmdOnetime(int device_family_index);
	vk::CommandPool getCmdPool(CmdPoolType type, int device_family_index)const;

	void resetPool(std::shared_ptr<btr::Executer>& executer);
	void enque(vk::UniqueCommandBuffer&& cmd, uint32_t family_index);
};
