#pragma once

#include <array>
#include <vector>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/sGlobal.h>

struct cCmdPool
{
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
	};
	struct CmdPoolPerThread
	{
		std::vector<CmdPoolPerFamily>	m_per_family;
	};
	std::vector<CmdPoolPerThread> m_per_thread;

	vk::CommandBuffer getCmdOnetime(int device_family_index)const;
	vk::CommandPool getCmdPool(CmdPoolType type, int device_family_index)const;

	static std::shared_ptr<cCmdPool> MakeCmdPool(cGPU& gpu);
};
