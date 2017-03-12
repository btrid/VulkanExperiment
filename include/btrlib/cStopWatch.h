#pragma once

#pragma once

#include <chrono>
#include <string>
class cStopWatch
{
	std::chrono::system_clock::time_point m_start;
public:
	cStopWatch()
	{
		m_start = std::chrono::system_clock::now();
	}
	~cStopWatch()
	{
	}

	float getElapsedTimeAsMicroSeconds()
	{
		auto finish = std::chrono::system_clock::now();
		long long deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(finish - m_start).count();
		float f = static_cast<float>(deltaTime);
		m_start = finish;
		return f;
	}

	float getElapsedTimeAsSeconds()
	{
		return getElapsedTimeAsMicroSeconds() / 1000.f / 1000.f;
	}


};

