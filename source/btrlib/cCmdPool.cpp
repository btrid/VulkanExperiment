#include <btrlib/cCmdPool.h>
#include <btrlib/Context.h>

cCmdPool::cCmdPool(const std::shared_ptr<btr::Context>& context)
{
	m_context = context;

	m_cmd_pool_system.resize(std::thread::hardware_concurrency());
	for (auto& per_thread : m_cmd_pool_system)
	{
		vk::CommandPoolCreateInfo cmd_pool_compiled;
		cmd_pool_compiled.queueFamilyIndex = (uint32_t)0;
		cmd_pool_compiled.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		per_thread = context->m_device.createCommandPoolUnique(cmd_pool_compiled);

	}
	m_cmd.resize(std::thread::hardware_concurrency());
	for (auto& per_thread : m_cmd)
	{ 
		for (auto& per_frame : per_thread)
		{
			vk::CommandPoolCreateInfo cmd_pool_onetime;
			cmd_pool_onetime.queueFamilyIndex = (uint32_t)0;
			cmd_pool_onetime.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
			per_frame.m_cmd_pool = context->m_device.createCommandPoolUnique(cmd_pool_onetime);
		}
	}

	m_tls_cmds.resize(std::thread::hardware_concurrency());
	
}



vk::CommandPool cCmdPool::getCmdPool(cCmdPool::CmdPoolType type, int device_family_index) const
{
	switch (type)
	{
	case CMD_POOL_TYPE_ONETIME:
		return m_cmd[sThreadLocal::Order().getThreadIndex()][sGlobal::Order().getCurrentFrame()].m_cmd_pool.get();
	case CMD_POOL_TYPE_COMPILED:
	default:
		return m_cmd_pool_system[sThreadLocal::Order().getThreadIndex()].get();
	}
}

void cCmdPool::resetPool()
{
	for (auto& cmds : m_cmd )
	{
		auto& c = cmds[sGlobal::Order().getCurrentFrame()];
		if (!c.m_cmd_onetime_deleter.empty())
		{
			m_context->m_device.resetCommandPool(c.m_cmd_pool.get(), vk::CommandPoolResetFlagBits::eReleaseResources);
			c.m_cmd_onetime_deleter.clear();
		}
	}
}

std::vector<vk::CommandBuffer> cCmdPool::submit()
{
	auto cmds = m_tls_cmds;
	m_tls_cmds = std::vector<vk::CommandBuffer>(std::thread::hardware_concurrency());

	cmds.erase(std::remove_if(cmds.begin(), cmds.end(), [&](auto& d) { return !d; }), cmds.end());
	for (auto& c : cmds)
	{
		c.end();
	}
	return cmds;

}

vk::CommandBuffer cCmdPool::allocCmdImpl()
{
	vk::CommandBufferAllocateInfo cmd_buffer_info;
	cmd_buffer_info.commandBufferCount = 1;
	cmd_buffer_info.commandPool = getCmdPool(CMD_POOL_TYPE_ONETIME, 0);
	cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
	auto cmd_unique = std::move(m_context->m_device.allocateCommandBuffersUnique(cmd_buffer_info)[0]);
	auto cmd = cmd_unique.get();
	m_cmd[sThreadLocal::Order().getThreadIndex()][sGlobal::Order().getCurrentFrame()].m_cmd_onetime_deleter.push_back(std::move(cmd_unique));

	vk::CommandBufferBeginInfo begin_info;
	begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	cmd.begin(begin_info);

	return cmd;
}

vk::CommandBuffer cCmdPool::allocCmdOnetime(int device_family_index, const char* name /*= nullptr*/)
{
	auto cmd = allocCmdImpl();

#if USE_DEBUG_REPORT
	char buf[256];
	sprintf_s(buf, "Onetime CMD %d", m_cmd[sThreadLocal::Order().getThreadIndex()][sGlobal::Order().getCurrentFrame()].m_cmd_onetime_deleter.size());
	vk::DebugUtilsObjectNameInfoEXT name_info;
	name_info.objectType = vk::ObjectType::eCommandBuffer;
	name_info.pObjectName = name ? name : buf;
	name_info.objectHandle = reinterpret_cast<uint64_t &>(cmd);
	m_context->m_device.setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
#endif

	return cmd;
}

vk::CommandBuffer cCmdPool::allocCmdTempolary(uint32_t device_family_index, const char* name /*= nullptr*/)
{
	auto& cmd = m_tls_cmds[sThreadLocal::Order().getThreadIndex()];
	if (!cmd)
	{
		cmd = allocCmdImpl();

#if USE_DEBUG_REPORT
		char buf[256];
		sprintf_s(buf, "TlsTempolary CMD %d", m_cmd[sThreadLocal::Order().getThreadIndex()][sGlobal::Order().getCurrentFrame()].m_cmd_onetime_deleter.size());
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.objectType = vk::ObjectType::eCommandBuffer;
		name_info.pObjectName = name ? name : buf;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(cmd);
		m_context->m_device.setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
#endif
	}
	return cmd;
}
