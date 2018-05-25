#pragma once
#include <013_2DGI/SV2D/SV2D.h>

namespace sv2d
{

struct SV2DClear
{
	SV2DClear(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<SV2DContext>& pm2d_context)
	{
		m_context = context;
		m_sv2d_context = pm2d_context;
	}
	void execute(vk::CommandBuffer cmd)
	{
		{
			// clear
			cmd.fillBuffer(m_sv2d_context->b_fragment_buffer.getInfo().buffer, m_sv2d_context->b_fragment_buffer.getInfo().offset, m_sv2d_context->b_fragment_buffer.getInfo().range, 0u);

			ivec4 emissive = ivec4{ 0,1,1,0 };
			static_assert(array_length(emissive) == SV2DContext::_BounceNum, "");
			cmd.updateBuffer(m_sv2d_context->b_emission_counter.getInfo().buffer, m_sv2d_context->b_emission_counter.getInfo().offset, sizeof(emissive), &emissive);
		}
	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<SV2DContext> m_sv2d_context;
};

}
