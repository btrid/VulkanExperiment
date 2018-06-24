#pragma once
#include <013_2DGI/GI2D/GI2DContext.h>

namespace gi2d
{

struct GI2DClear
{
	GI2DClear(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
	}
	void execute(vk::CommandBuffer cmd)
	{
		{
			// clear
			cmd.fillBuffer(m_gi2d_context->b_fragment_buffer.getInfo().buffer, m_gi2d_context->b_fragment_buffer.getInfo().offset, m_gi2d_context->b_fragment_buffer.getInfo().range, 0u);

			ivec4 emissive[] = { { 0,1,1,0 },{ 0,1,1,0 },{ 0,1,1,0 },{ 0,1,1,0 } };
			static_assert(array_length(emissive) == GI2DContext::_BounceNum, "");
			cmd.updateBuffer(m_gi2d_context->b_emission_counter.getInfo().buffer, m_gi2d_context->b_emission_counter.getInfo().offset, sizeof(emissive), emissive);
		}
	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
};

}
