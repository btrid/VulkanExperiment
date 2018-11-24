#pragma once
#include <013_2DGI/GI2D/GI2DContext.h>

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
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			// clear
			cmd.fillBuffer(m_gi2d_context->b_fragment.getInfo().buffer, m_gi2d_context->b_fragment.getInfo().offset, m_gi2d_context->b_fragment.getInfo().range, 0u);
			cmd.fillBuffer(m_gi2d_context->b_grid_counter.getInfo().buffer, m_gi2d_context->b_grid_counter.getInfo().offset, m_gi2d_context->b_grid_counter.getInfo().range, 0);

		}
	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
};
