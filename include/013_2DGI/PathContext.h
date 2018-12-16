#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <random>
#include <cstdint>
#include<fstream>

#include <btrlib/DefineMath.h>
#include <btrlib/cStopWatch.h>


struct PathContextCPU
{
	struct Description
	{
		ivec2 m_size;
		ivec2 m_start;
		ivec2 m_finish;
	};
	PathContextCPU(const Description& desc)
	{
		m_desc = desc;
	}


	bool isPath(const ivec2 i)const
	{
		auto wh_m = m_desc.m_size >> 3;
		ivec2 m = i >> 3;
		ivec2 c = i - (m << 3);
		return (m_field[m.x + m.y*wh_m.x] & (1ull << (c.x + c.y * 8))) == 0;
	}

	Description m_desc;
	std::vector<uint64_t> m_field;

};

std::vector<uint64_t> pathmake_noise(int sizex, int sizey);
std::vector<char> pathmake_maze_(int sizex, int sizey);
std::vector<uint64_t> pathmake_maze(int sizex, int sizey);

