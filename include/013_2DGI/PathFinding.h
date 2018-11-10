#pragma once

#include <vector>
#include <queue>
#include <random>
#include <cstdint>
#include<fstream>

#include <btrlib/DefineMath.h>
struct PathFinding
{
	PathFinding()
	{
		cStopWatch time;
//		auto a = 1 << 15;
//		auto a = 1 << 8;
		auto a = 1024;
		auto aa = a >> 3;
		m_info.m_size = ivec2(a);
//		m_start = ivec2(256, 4);
//		m_finish = ivec2(667, 1000);
		m_info.m_start = ivec2(11, 4);
		m_info.m_finish = ivec2(a - 10, a - 10);
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

	}

	void write()
	{
		auto map_size = m_info.m_size >> 3;
		FILE* file;
		fopen_s(&file, "test.txt", "w");
		for (uint32_t y = 0; y < m_info.m_size.y; y++)
		{
			std::vector<char> output(m_info.m_size.x + 1);
			for (uint32_t x = 0; x < m_info.m_size.y; x++)
			{
				ivec2 m = ivec2(x, y) >> 3;
				ivec2 c = ivec2(x, y) - (m << 3);
				output[x] = (m_field[m.x + m.y*map_size.x] & (1ull << (c.x + c.y * 8))) != 0 ? '#' : ' ';
			}
			output[m_info.m_size.x] = '\0';
			fprintf(file, "%s\n", output.data());
		}
		fclose(file);
	}
	void writeSolvePath(const std::vector<uint64_t>& solver, const std::string& filename)
	{
		auto map_size = m_info.m_size >> 3;
		FILE* file;
		fopen_s(&file, filename.c_str(), "w");
		for (uint32_t y = 0; y < m_info.m_size.y; y++)
		{
			std::vector<char> output(m_info.m_size.x + 1);
			for (uint32_t x = 0; x < m_info.m_size.y; x++)
			{
				ivec2 m = ivec2(x, y) >> 3;
				ivec2 c = ivec2(x, y) - (m << 3);
				uint64_t bit = 1ull << (c.x + c.y * 8);
				char input = (m_field[m.x + m.y*map_size.x] & bit) != 0 ? '#' : ' ';
				input = (solver[m.x + m.y*map_size.x] & bit) != 0 ? '@' : input;
				output[x] = input;
			}
			output[m_info.m_size.x] = '\0';
			fprintf(file, "%s\n", output.data());
		}
		fclose(file);
	}

	struct Info
	{
		ivec2 m_size;
		ivec2 m_start;
		ivec2 m_finish;
	};
	Info m_info;
	std::vector<uint64_t> m_field;

	bool isPath(const ivec2 i)const
	{
		auto wh_m = m_info.m_size >> 3;
		ivec2 m = i >> 3;
		ivec2 c = i - (m << 3);
		return (m_field[m.x + m.y*wh_m.x] & (1ull << (c.x + c.y * 8))) == 0;

	}
};

struct Solver
{

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

	std::vector<uint64_t> executeSolve(const PathFinding& path)const
	{
		cStopWatch time;

		auto aa = path.m_info.m_size >> 3;
		std::vector<uint64_t> result(aa.x*aa.y);

		std::vector<CloseNode> close(path.m_info.m_size.x * path.m_info.m_size.y);
		std::deque<OpenNode> open;
		open.push_back({ path.m_info.m_start.x, path.m_info.m_start.y, 0, -1 });
		while (!open.empty())
		{
			OpenNode n = open.front();
			open.pop_front();

			// 右
			auto current = n.x + n.y*path.m_info.m_size.x;

			if (all(equal(ivec2(n.x, n.y), path.m_info.m_finish)))
			{
				printf("solve time %6.4fs\n", time.getElapsedTimeAsSeconds());

				auto map_size = path.m_info.m_size >> 3;

				// ゴール着いた
				int32_t index = path.m_info.m_finish.x + path.m_info.m_finish.y*path.m_info.m_size.x;
				ivec2 index2 = ivec2(index % path.m_info.m_size.x, index / path.m_info.m_size.y);
				auto map_index = index2 >> 3;
				auto map_index_sub = index2 - (map_index << 3);

				for (;;)
				{

					result[map_index.x + map_index.y*map_size.x] |= 1ull << (map_index_sub.x + map_index_sub.y * 8);

					index = close[index].parent;
					index2 = ivec2(index % path.m_info.m_size.x, index / path.m_info.m_size.y);
					if (all(equal(path.m_info.m_start, index2)))
					{
						printf("succeed\n");
						return result;
					}
					map_index = index2 >> 3;
					map_index_sub = index2 - (map_index << 3);
				}
			}
			if (n.x < path.m_info.m_size.x - 1)
			{
				_setNode(path, n.x + 1, n.y, current, n.cost + 1, open, close);
			}
			// 左
			if (n.x > 0)
			{
				_setNode(path, n.x - 1, n.y, current, n.cost + 1, open, close);
			}
			// 下
			if (n.y < path.m_info.m_size.y - 1)
			{
				_setNode(path, n.x, n.y + 1, current, n.cost + 1, open, close);
			}
			// 上
			if (n.y > 0)
			{
				_setNode(path, n.x, n.y - 1, current, n.cost + 1, open, close);
			}
		}

		// 見つからなかった
		printf("failed\n");
		return result;
	}
	void _setNode(const PathFinding& path, int x, int y, int parent, int cost, std::deque<OpenNode>& open, std::vector<CloseNode>& close)const
	{
		{
			auto wh_m = path.m_info.m_size >> 3;
			ivec2 m = ivec2(x, y) >> 3;
			ivec2 c = ivec2(x, y) - (m << 3);
			if ((path.m_field[m.x + m.y*wh_m.x] & (1ull << (c.x + c.y * 8))) != 0)
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

			auto& cn = close[x + y * path.m_info.m_size.x];
			if (cn.cost >= 0 && cost >= cn.cost)
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

};

struct Solver2
{


	struct Node
	{
		int32_t cost;
		int32_t parent;
		uint32_t searched : 4; //!<上下左右の探索を行ったか？
		uint32_t condition : 5;
		uint32_t p : 23;
		Node()
			: cost(-1)
			, parent(-1)
			, searched(0)
			, condition(0)
		{}

		void precompute(const ivec2& c, const PathFinding& path)
		{
			ivec2 offset[4] = { {-1, 0}, {1, 0}, {0, 1}, {0, -1}, };
			// 事前計算
			condition = 1 << 4;
			for (int i = 0; i < 4; i++)
			{
				auto _n = c + offset[i];
				if (any(lessThan(_n, ivec2(0))) || any(greaterThanEqual(_n, path.m_info.m_size)))
				{
					condition |= 1 << i;
					continue;
				}
				if (!path.isPath(_n))
				{
					condition |= 1 << i;
					continue;
				}
			}

		}
	};

	std::vector<uint64_t> executeSolve(const PathFinding& path)const
	{
		cStopWatch time;

		ivec2 offset[4] = { {-1, 0}, {1, 0}, {0, 1}, {0, -1}, };

		auto aa = path.m_info.m_size >> 3;
		std::vector<uint64_t> result(aa.x*aa.y);
		std::vector<Node> node(path.m_info.m_size.x * path.m_info.m_size.y);

		ivec2 current = path.m_info.m_start;
		node[current.x + current.y * path.m_info.m_size.x].cost = 0;
		node[current.x + current.y * path.m_info.m_size.x].precompute(current, path);
		node[current.x + current.y * path.m_info.m_size.x].searched |= node[current.x + current.y * path.m_info.m_size.x].condition & 0b1111;

		for (;;)
		{
			if (all(equal(current, path.m_info.m_finish)))
			{
				printf("solve time %6.4fs\n", time.getElapsedTimeAsSeconds());

				auto map_size = path.m_info.m_size >> 3;

				// ゴール着いた
				int32_t index = path.m_info.m_finish.x + path.m_info.m_finish.y*path.m_info.m_size.x;
				ivec2 index2 = ivec2(index % path.m_info.m_size.x, index / path.m_info.m_size.y);
				auto map_index = index2 >> 3;
				auto map_index_sub = index2 - (map_index << 3);

				for (;;)
				{

					result[map_index.x + map_index.y*map_size.x] |= 1ull << (map_index_sub.x + map_index_sub.y * 8);

					index = node[index].parent;
					index2 = ivec2(index % path.m_info.m_size.x, index / path.m_info.m_size.x);
					if (all(equal(path.m_info.m_start, index2)))
					{
						printf("succeed\n");
						return result;
					}
					map_index = index2 >> 3;
					map_index_sub = index2 - (map_index << 3);
				}
			}

			auto& current_node = node[current.x + current.y * path.m_info.m_size.x];
			if ((current_node.searched & 0b1111) == 0b1111)
			{
				// 全部探索済みなので親を辿る
				if (current_node.parent == -1)
				{
					// 親がないならもうない
					break;
				}
				current = ivec2(current_node.parent%path.m_info.m_size.x, current_node.parent/ path.m_info.m_size.x);
			}
			else
			{
				uvec4 dist = uvec4(-1);
				for (int i = 0; i < 4; i++)
				{
					if ((current_node.searched & (1 << i)) != 0) {
						continue;
					}
					auto d = glm::abs(current + offset[i] - path.m_info.m_finish);
					dist[i] = d.x + d.y;
				}

				int32_t dir = dist[0] < dist[1] ? 0 : 1;
				dir = dist[dir] < dist[2] ? dir : 2;
				dir = dist[dir] < dist[3] ? dir : 3;

				current_node.searched |= 1 << dir;

				auto next = current + offset[dir];
				auto& next_node = node[next.x + next.y * path.m_info.m_size.x];
				if (next_node.cost >= 0 && current_node.cost+1 >= next_node.cost)
				{
					// すでに調査済み
				}
				else
				{
					// もっと早くアクセスする方法が見つかったor新しく探索するので更新
					next_node.cost = current_node.cost+1;
					next_node.parent = current.x + current.y * path.m_info.m_size.x;

					// 親はアクセスする必要ない
					next_node.searched = 1 << (dir ^ 1);
					if (next_node.condition == 0)
					{
						next_node.precompute(next, path);
					}
					next_node.searched |= next_node.condition&0b1111;

					current = next;
				}



			}
		}

		// 見つからなかった
		printf("failed\n");
		return result;
	}

};