#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <random>
#include <cstdint>
#include<fstream>

#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
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

struct PathContext
{
	struct PathNode
	{
		uint cost;
	};

	PathContext(const std::shared_ptr<btr::Context>& context, const PathContextCPU& path)
	{
		m_context = context;
// 		b_node = context->m_storage_memory.allocateMemory<PathNode>({ path.m_desc.m_size.x*path.m_desc.m_size.y, {} });
// 		b_open = context->m_storage_memory.allocateMemory<uint32_t>({ 1, {} });
// 		b_open_counter = context->m_storage_memory.allocateMemory<uvec4>({ 1024, {} });

	}

	std::shared_ptr<btr::Context> m_context;

	btr::BufferMemoryEx<PathNode> b_node;
	btr::BufferMemoryEx<uint32_t> b_space;
	btr::BufferMemoryEx<uvec4> b_space_counter;
};

std::vector<uint64_t> pathmake_noise(int sizex, int sizey);
std::vector<char> pathmake_maze_(int sizex, int sizey);
std::vector<uint64_t> pathmake_maze(int sizex, int sizey);

