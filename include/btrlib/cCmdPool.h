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
template<typename T> using PerThread = std::vector<T>;
template<typename T> using PerFamilyIndex = std::vector<T>;
struct cCmdPool
{
	static std::shared_ptr<cCmdPool> MakeCmdPool(cGPU& gpu);

	enum CmdPoolType
	{
		CMD_POOL_TYPE_ONETIME,	//!< 1�t���[���Ɉ�xpool�����Z�b�g����� 
		CMD_POOL_TYPE_TEMPORARY,		//!< ���t���[���ɓn����cmd���g����B�����ŊǗ����� 
		CMD_POOL_TYPE_COMPILED,		//!< cmd�͎����ŊǗ�����
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
	std::mutex m_cmd_queue_mutex;

	vk::CommandBuffer getCmdOnetime(int device_family_index);
	vk::CommandPool getCmdPool(CmdPoolType type, int device_family_index)const;

	void resetPool(std::shared_ptr<btr::Executer>& executer);
	void enqueCmd(vk::UniqueCommandBuffer&& cmd, uint32_t family_index);
	PerFamilyIndex<std::vector<vk::UniqueCommandBuffer>> submitCmd();
};
