#pragma once
#include <btrlib/Define.h>

struct PM2DPipeline
{
	virtual void execute(vk::CommandBuffer cmd) = 0;
};
