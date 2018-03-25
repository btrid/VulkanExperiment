#pragma once

struct PM2DPipeline
{
	virtual void execute(vk::CommandBuffer cmd) = 0;
};
