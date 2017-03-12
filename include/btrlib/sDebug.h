#pragma once

//module btr;
#include <functional>
#include <string>
#include <sstream>
#include <btrlib/Singleton.h>


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
	void print(int flag, const char* msg, Arg... arg)
	{
		char hoge[256];
		sprintf_s(hoge, msg, arg...);
		printf(hoge);

		assert((flag & ACTION_ASSERTION) == 0);
	}
};

template<typename... Arg>
void ERROR_PRINT(const char* msg, Arg... arg) { sDebug::Order().print(sDebug::FLAG_ERROR, msg, arg...); }
