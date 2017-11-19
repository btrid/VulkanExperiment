#pragma once

#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

struct sUISystem : Singleton<sUISystem>
{
	friend Singleton<sUISystem>;

	void setup(const std::shared_ptr<btr::Context>& context);
};

