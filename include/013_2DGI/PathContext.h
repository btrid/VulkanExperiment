#pragma once

#include <vector>
#include <queue>
#include <random>
#include <cstdint>

#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <btrlib/cStopWatch.h>

#include <013_2DGI/GI2D/GI2DContext.h>


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
	struct SparseMap
	{
		uint32_t index;
		uint32_t child_index;
		uint64_t map;
	};
	struct PathNode
	{
		uint32_t cost;
	};

	PathContext(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
 		b_node = context->m_storage_memory.allocateMemory<PathNode>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y, {} });
		b_node_counter = context->m_storage_memory.allocateMemory<uint32_t>({ 1, {} });
		b_node_hierarchy_counter = context->m_storage_memory.allocateMemory<uvec4>({ 4, {} });

	}

	std::shared_ptr<btr::Context> m_context;

	btr::BufferMemoryEx<PathNode> b_node;
	btr::BufferMemoryEx<uint32_t> b_node_counter;
	btr::BufferMemoryEx<uvec4> b_node_hierarchy_counter;
};

std::vector<uint64_t> pathmake_noise(int sizex, int sizey);
std::vector<char> pathmake_maze_(int sizex, int sizey);
std::vector<uint64_t> pathmake_maze(int sizex, int sizey);

