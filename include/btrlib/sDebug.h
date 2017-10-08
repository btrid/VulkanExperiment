#pragma once

//module btr;
#include <functional>
#include <string>
#include <sstream>
#include <cassert>
#include <btrlib/Singleton.h>


namespace vk {
	class Device;
	class Fence;
}
class sDebug : public Singleton<sDebug>
{
	friend Singleton<sDebug>;
	std::function<void(int, const std::string& msg)> m_print_function;
protected:
	
	sDebug() = default;
	~sDebug() = default;

public:
	enum
	{
		FLAG_ERROR	= 1 << 0,
		FLAG_WARNING = 1 << 1,
		FLAG_LOG		= 1 << 2,
		FLAG_PERFORMANCE = 1<< 3,
		SOURCE_MODEL = 1 << 4,
		ACTION_ASSERTION = 1<<5,
	};

	template<typename... Arg>
	void print(int flag, const char* msg, Arg... arg)const
	{
		char hoge[256];
		sprintf_s(hoge, msg, arg...);
		printf(hoge);

		assert((flag & ACTION_ASSERTION) == 0);
	}

	//! どうしても待つ必要がある場合に呼ぶ。これを呼び出すのは設計ミスな気がする。
	void waitFence(const vk::Device& device, const vk::Fence& fence)const;
};

template<typename... Arg>
void ERROR_PRINT(const char* msg, Arg... arg) { sDebug::Order().print(sDebug::FLAG_ERROR, msg, arg...); }
