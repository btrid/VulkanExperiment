#pragma once
#include <013_2DGI/PM2D/PM2D.h>

namespace pm2d
{

struct PM2DClear
{
	PM2DClear(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context)
	{
		m_context = context;
		m_pm2d_context = pm2d_context;
	}
	void execute(vk::CommandBuffer cmd)
	{
		{
			// clear
			cmd.fillBuffer(m_pm2d_context->b_fragment_buffer.getInfo().buffer, m_pm2d_context->b_fragment_buffer.getInfo().offset, m_pm2d_context->b_fragment_buffer.getInfo().range, 0u);

			ivec4 emissive[] = { { 0,1,1,0 },{ 0,1,1,0 },{ 0,1,1,0 },{ 0,1,1,0 } };
			static_assert(array_length(emissive) == PM2DContext::_BounceNum, "");
			cmd.updateBuffer(m_pm2d_context->b_emission_counter.getInfo().buffer, m_pm2d_context->b_emission_counter.getInfo().offset, sizeof(emissive), emissive);
		}
	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;
};

}
