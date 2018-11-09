#pragma once

#include <vector>
#include <queue>
#include <random>
#include <cstdint>
#include<fstream>

#include <btrlib/DefineMath.h>
struct TestField
{
	TestField()
	{
		cStopWatch time;
		auto a = 1 << 15;
//		auto a = 1 << 10;
		auto aa = a >> 3;
		m_size = ivec2(a);
		m_field.resize(aa*aa);
		for (int y = 0; y < aa; y++)
		{
			for (int x = 0; x < aa; x++)
			{
				uint64_t m = 0;
				for (int i = 0; i < 64; i++)
				{
//					m |= glm::simplex(vec2(x * 8 + i % 8, y * 8 + i / 8) / 80.f) >= abs(0.2f) ? (1ull << i) : 0ull;
					m |= glm::simplex(vec2(x * 8 + i % 8, y * 8 + i / 8) / 256.f) >= abs(0.5f) ? (1ull << i) : 0ull;
				}
				m_field[y*aa + x] = m;
			}
		}
		printf("make time %6.4fs\n", time.getElapsedTimeAsSeconds());

//		m_start = ivec2(256, 4);
	//	m_finish = ivec2(667, 1000);
		m_start = ivec2(256, 4);
		m_finish = ivec2(a - 100, a - 100);
	}

	void write()
	{
		auto map_size = m_size >> 3;
		FILE* file;
		fopen_s(&file, "test.txt", "w");
		for (uint32_t y = 0; y < m_size.y; y++)
		{
			std::vector<char> output(m_size.x + 1);
			for (uint32_t x = 0; x < m_size.y; x++)
			{
				ivec2 m = ivec2(x, y) >> 3;
				ivec2 c = ivec2(x, y) - (m << 3);
				output[x] = (m_field[m.x + m.y*map_size.x] & (1ull << (c.x + c.y * 8))) != 0 ? '#' : ' ';
			}
			output[m_size.x] = '\0';
			fprintf(file, "%s\n", output.data());
		}
		fclose(file);
	}
	void writeSolvePath(const std::vector<uint64_t>& solver)
	{
		auto map_size = m_size >> 3;
		FILE* file;
		fopen_s(&file, "path.txt", "w");
		for (uint32_t y = 0; y < m_size.y; y++)
		{
			std::vector<char> output(m_size.x + 1);
			for (uint32_t x = 0; x < m_size.y; x++)
			{
				ivec2 m = ivec2(x, y) >> 3;
				ivec2 c = ivec2(x, y) - (m << 3);
				uint64_t bit = 1ull << (c.x + c.y * 8);
				char input = (m_field[m.x + m.y*map_size.x] & bit) != 0 ? '#' : ' ';
				input = (solver[m.x + m.y*map_size.x] & bit) != 0 ? '@' : input;
				output[x] = input;
			}
			output[m_size.x] = '\0';
			fprintf(file, "%s\n", output.data());
		}
		fclose(file);
	}

	struct OpenNode
	{
		int32_t x;
		int32_t y;
		int32_t cost;
		int32_t parent;
	};
	struct CloseNode
	{
		int32_t cost;
		int32_t parent;
		CloseNode()
			: cost(-1)
			, parent(-1)
		{}
	};
	std::vector<uint64_t> solve()const
	{
		cStopWatch time;

		auto aa = m_size>>3;
		std::vector<uint64_t> path(aa.x*aa.y);
			
		std::vector<CloseNode> close(m_size.x * m_size.y);
		std::deque<OpenNode> open;
		open.push_back({ m_start.x, m_start.y, 0, -1 });
		while (!open.empty())
		{
			OpenNode n = open.front();
			open.pop_front();

			// 右
			auto current = n.x + n.y*m_size.x;

			if (all(equal(ivec2(n.x, n.y), m_finish)))
			{
				printf("solve time %6.4fs\n", time.getElapsedTimeAsSeconds());

				auto map_size = m_size >> 3;

				// ゴール着いた
				int32_t index = m_finish.x + m_finish.y*m_size.x;
				ivec2 index2 = ivec2(index % m_size.x, index / m_size.y);
				auto map_index = index2 >> 3;
				auto map_index_sub = index2 - (map_index<<3);

				for (;;)
				{

					path[map_index.x + map_index.y*map_size.x] |= 1ull<<(map_index_sub.x+map_index_sub.y*8);

					index = close[index].parent;
					index2 = ivec2(index % m_size.x, index / m_size.y);
					if (all(equal(m_start, index2)))
					{
						return path;
					}
					map_index = index2 >> 3;
					map_index_sub = index2 - (map_index << 3);
				}
			}
			if (n.x < m_size.x-1)
			{
				_setNode(n.x + 1, n.y, current, n.cost + 1, open, close);
			}
			// 左
			if (n.x > 0)
			{
				_setNode(n.x - 1, n.y, current, n.cost + 1, open, close);
			}
			// 下
			if (n.y < m_size.y - 1)
			{
				_setNode(n.x, n.y + 1, current, n.cost + 1, open, close);
			}
			// 上
			if (n.y > 0)
			{
				_setNode(n.x, n.y - 1, current, n.cost + 1, open, close);
			}
		}

		// 見つからなかった
		return path;
	}
	void _setNode(int x, int y, int parent, int cost, std::deque<OpenNode>& open, std::vector<CloseNode>& close)const
	{
		{
			auto wh_m = m_size >> 3;
			ivec2 m = ivec2(x, y) >> 3;
			ivec2 c = ivec2(x, y) - (m << 3);
			if ((m_field[m.x + m.y*wh_m.x] & (1ull << (c.x + c.y * 8))) != 0)
			{
				// ふさがれてるのでダメ
				return;
			}

		}

		do
		{
			auto it = std::find_if(open.begin(), open.end(), [x, y](const OpenNode& n) { return x == n.x && y == n.y; });
			if (it != open.end()) 
			{
				// openの中に見つかった
				if (cost >= it->cost) 
				{
					break;
				}
				else
				{
					it->cost = cost;
					it->parent = parent;
					break;
				}
			}

			auto& cn = close[x+y*m_size.x];
			if (cn.cost >= 0 && cost > cn.cost) 
			{
				break;
			}
			else
			{
				// もっと早くアクセスする方法が見つかったので更新
				cn.cost = cost;
				cn.parent = parent;
			}
			open.push_back({ x, y, cost, parent });

		} while (false);

	}


	ivec2 m_size;
	ivec2 m_start;
	ivec2 m_finish;
	std::vector<uint64_t> m_field;
};