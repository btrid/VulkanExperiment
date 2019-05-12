#pragma once
#include <013_2DGI/PathContext.h>
#include <btrlib/cStopWatch.h>

struct PathSolver
{
#define offset_def ivec2 offset[4] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1}, }
	struct Node
	{
		uint32_t cost;
		uint32_t parent;
		uint32_t is_open : 1;
		uint32_t is_precomputed : 1;
		uint32_t condition : 4; //!< 探索する必要があるか
		uint32_t p : 26;
		Node()
			: cost(-1)
			, parent(-1)
			, is_open(0)
			, is_precomputed(0)
			, condition(0)
		{}

		void precompute(const ivec2& c, const PathContextCPU& path)
		{
			offset_def;
			// 事前計算
			is_precomputed = 1;
			for (int i = 0; i < 4; i++)
			{
				auto _n = c + offset[i];
				if (any(lessThan(_n, ivec2(0))) || any(greaterThanEqual(_n, path.m_desc.m_size)))
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

	std::vector<uint64_t> executeSolve(const PathContextCPU& path)const
	{
		cStopWatch time;

		offset_def;
		auto aa = path.m_desc.m_size >> 3;
		std::vector<uint64_t> result(aa.x*aa.y);

		std::vector<Node> close(path.m_desc.m_size.x * path.m_desc.m_size.y);
		std::deque<uint32_t> open;
		open.push_back(path.m_desc.m_start.x + path.m_desc.m_start.y * path.m_desc.m_size.x);
		{
			auto& start = close[open.front()];
			start.is_open = 1;
			start.precompute(path.m_desc.m_start, path);
		}
		while (!open.empty())
		{
			uint32_t node_index = open.front();
			Node& node = close[node_index];
			open.pop_front();

			ivec2 n = ivec2(node_index%path.m_desc.m_size.x, node_index / path.m_desc.m_size.x);

			if (all(equal(ivec2(n.x, n.y), path.m_desc.m_finish)))
			{
				printf("solve time %6.4fs\n", time.getElapsedTimeAsSeconds());

				auto map_size = path.m_desc.m_size >> 3;

				// ゴール着いた
				int32_t index = path.m_desc.m_finish.x + path.m_desc.m_finish.y*path.m_desc.m_size.x;
				ivec2 index2 = ivec2(index % path.m_desc.m_size.x, index / path.m_desc.m_size.y);
				auto map_index = index2 >> 3;
				auto map_index_sub = index2 - (map_index << 3);

				for (;;)
				{

					result[map_index.x + map_index.y*map_size.x] |= 1ull << (map_index_sub.x + map_index_sub.y * 8);

					index = close[index].parent;
					index2 = ivec2(index % path.m_desc.m_size.x, index / path.m_desc.m_size.y);
					if (all(equal(path.m_desc.m_start, index2)))
					{
						printf("succeed\n");
						return result;
					}
					map_index = index2 >> 3;
					map_index_sub = index2 - (map_index << 3);
				}
			}

			for (int i = 0; i < 4; i++)
			{
				if ((node.condition & (1 << i)) != 0) {
					continue;
				}
				_setNode(path, n.x + offset[i].x, n.y + offset[i].y, node_index, node.cost + 1, open, close);
			}

			node.is_open = 0;

		}

		// 見つからなかった
		printf("solve time %6.4fs\n", time.getElapsedTimeAsSeconds());
		printf("failed\n");
		return result;
	}

	std::vector<uint32_t> executeMakeVectorField(const PathContextCPU& path)const
	{
		cStopWatch time;
		offset_def;

		std::vector<Node> close(path.m_desc.m_size.x * path.m_desc.m_size.y);
		std::deque<uint32_t> open;
		open.push_back(path.m_desc.m_start.x + path.m_desc.m_start.y * path.m_desc.m_size.x);
		{
			auto& start = close[open.front()];
			start.is_open = 1;
			start.precompute(path.m_desc.m_start, path);
		}
		while (!open.empty())
		{
			uint32_t node_index = open.front();
			Node& node = close[node_index];
			open.pop_front();

			ivec2 n = ivec2(node_index%path.m_desc.m_size.x, node_index / path.m_desc.m_size.x);

			for (int i = 0; i < 4; i++)
			{
				if ((node.condition & (1 << i)) != 0) {
					continue;
				}
				_setNode(path, n.x + offset[i].x, n.y + offset[i].y, node_index, node.cost + 1, open, close);
			}
			node.is_open = 0;

		}

		printf("solve time %6.4fms\n", time.getElapsedTimeAsMilliSeconds());
		std::vector<uint32_t> result(path.m_desc.m_size.x*path.m_desc.m_size.y);
		for (uint32_t y = 0; y < path.m_desc.m_size.y; y++)
		{
			for (uint32_t x = 0; x < path.m_desc.m_size.x; x++)
			{
				uint32_t i = y * path.m_desc.m_size.x + x;
				result[i] = close[i].cost;
			}
		}
		return result;
	}
	void _setNode(const PathContextCPU& path, int x, int y, uint32_t parent, uint32_t cost, std::deque<uint32_t>& open, std::vector<Node>& close)const
	{
		auto& cn = close[x + y * path.m_desc.m_size.x];

		// 既に探索済み
		if (cost >= cn.cost)
		{
			return;
		}

		if (cn.is_open == 1)
		{
			// openの中に見つかった
			cn.cost = cost;
			cn.parent = parent;
			return;
		}
		else
		{
			// もっと早くアクセスする方法が見つかったので更新
			cn.cost = cost;
			cn.parent = parent;
			cn.is_open = 1;
			if (cn.is_precomputed == 0)
			{
				cn.precompute(ivec2(x, y), path);
			}
			open.push_back(x + y * path.m_desc.m_size.x);
		}

	}

	enum Direction
	{
		UL,
		U,
		UR,
		R,
		DR,
		D,
		DL,
		L,
		All,
		Num,

		Bit_UL = 1 << UL,
		Bit_U = 1 << U,
		Bit_UR = 1 << UR,
		Bit_R = 1 << R,
		Bit_DR = 1 << DR,
		Bit_D = 1 << D,
		Bit_DL = 1 << DL,
		Bit_L = 1 << L,

		BIT_ALL = 255,

	};

	std::array<i16vec2, 8> neighor_list =
	{
		i16vec2{-1, -1},
		i16vec2{ 0, -1},
		i16vec2{ 1, -1},
		i16vec2{ 1,  0},
		i16vec2{ 1,  1},
		i16vec2{ 0,  1},
		i16vec2{-1,  1},
		i16vec2{-1,  0},
	};
	struct Explorer
	{
		uint8_t access_bit;
		uint8_t deny_bit;
	};
	std::array<Explorer, Num> explorer_list = 
	{
		Explorer{
			Bit_U | Bit_L | Bit_UR | Bit_UL | Bit_DR,
			Bit_UL | Bit_U | Bit_L,
		},
		Explorer{
			Bit_U | Bit_R | Bit_L,
			Bit_U,
		},
		Explorer{
			Bit_U | Bit_R | Bit_UR | Bit_UL | Bit_DR,
			Bit_UR | Bit_U | Bit_R ,
		},
		Explorer{
			Bit_R | Bit_U | Bit_D,
			Bit_R,
		},
		Explorer{
			Bit_D | Bit_R | Bit_DR | Bit_UR | Bit_DL,
			Bit_DR | Bit_D | Bit_R,
		},
		Explorer{
			Bit_D | Bit_R | Bit_L,
			Bit_D,
		},
		Explorer{
			Bit_D | Bit_L | Bit_DL | Bit_UL | Bit_DR,
			Bit_DL | Bit_D | Bit_L,
		},
		Explorer{
			Bit_L | Bit_U | Bit_D,
			Bit_L,
		},
		Explorer{
			255,
			0,
		},
	};
	struct OpenNode2
	{
		i16vec2 index;
		char dir_bit;
	};
	struct CloseNode2
	{
		i16vec2 parent;
		uint32_t is_open : 1;
		uint32_t is_closed: 1;
		uint32_t p : 30;
		float cost;
	};
	char getNeighborWall(const PathContextCPU& path, char dir, const i16vec2& pos)
	{
		char neighor = 0;
		for (int i = 0; i < 8;)
		{
			if (dir & (1 << i) == 0) 
			{
				// 見る必要ない
				continue; 
			}

			if (any(lessThan(neighor_list[i] + pos, i16vec2(0))) || any(greaterThanEqual(neighor_list[i] + pos, i16vec2(path.m_desc.m_size))))
			{
				// map外は壁としておく
				neighor |= (1 << i);
			}
			else
			{
				// 壁ならダメ
				neighor |= (!path.isPath(neighor_list[i] + pos)) ? (1 << i) : 0;
			}
		}
		return neighor;
	}
	char getNeighborPath(const PathContextCPU& path, char dir, const i16vec2& pos)
	{
		return ~getNeighborWall(path, dir, pos);
	}

	bool exploreStraight(const PathContextCPU& path, std::deque<OpenNode2>& open, const OpenNode2& node, int dir_type)
	{
		const auto& dir_ = neighor_list[dir_type];
		auto current = node.index;
		for (int i = 0; true; i++)
		{
			current += dir_;
			auto neighbor = getNeighborWall(path, (Direction)dir_type, current);

			if (btr::isOn(neighbor, explorer_list[dir_type].deny_bit))
			{
				// 直線は進行方向に進めなければ終了
				break;
			}
			btr::setOff(neighbor, explorer_list[dir_type].deny_bit);

			if (neighbor != 0)
			{
				// forced neighbor
				// 遮蔽物があるのでここから再捜査
				OpenNode2 open_node;
				open_node.index = current;
				open_node.dir_bit = (1<<dir_type);
				while(neighbor != 0)
				{
					int n_dir_type = glm::findLSB(neighbor);
					btr::setOff(neighbor, 1 << n_dir_type);

					open_node.dir_bit |= 1 << glm::min((dir_type + 1) % 8, (n_dir_type + 1) % 8);
				}
				open.push_back(open_node);
				return true;
			}
		}

		return false;

	}
	void exploreDiagonal(const PathContextCPU& path, const OpenNode2& node, int dir_type)
	{

	}
	void explore(const PathContextCPU& path, std::deque<OpenNode2>& open, const OpenNode2& node)
	{
		for (char path_ = getNeighborPath(path, node.dir_bit, node.index) ; path_ != 0;)
		{
			int dir_type = glm::findLSB(path_);
			btr::setOff(path_, 1 << dir_type);

			if ((dir_type %2) == 1)
			{
				exploreStraight(path, open, node, dir_type);
			}
			else 
			{
				exploreDiagonal(path, node, dir_type);
			}
		}
	}
	std::vector<uint32_t> executeMakeVectorField2(const PathContextCPU& path)const
	{
		cStopWatch time;

		std::vector<CloseNode2> close(path.m_desc.m_size.x * path.m_desc.m_size.y);
		std::deque<OpenNode2> open;
		open.push_back(OpenNode2{ i16vec2(path.m_desc.m_start.x, path.m_desc.m_start.y), BIT_ALL});
		{
			auto& start = close[open.front().index.x+ open.front().index.y*path.m_desc.m_size.x];
			start.is_open = 1;
		}
		while (!open.empty())
		{
			const OpenNode2& open_node = open.front();
			CloseNode2& node = close[open_node.index.x + open_node.index.y * path.m_desc.m_size.x];
			node.is_open = 0;
			node.is_closed = 1;
			open.pop_front();

		}

		printf("solve time %6.4fms\n", time.getElapsedTimeAsMilliSeconds());
		std::vector<uint32_t> result(path.m_desc.m_size.x*path.m_desc.m_size.y);
		for (uint32_t y = 0; y < path.m_desc.m_size.y; y++)
		{
			for (uint32_t x = 0; x < path.m_desc.m_size.x; x++)
			{
				uint32_t i = y * path.m_desc.m_size.x + x;
				result[i] = close[i].cost;
			}
		}
		return result;
	}

	void write(const PathContextCPU& path)
	{
		auto map_size = path.m_desc.m_size >> 3;
		FILE* file;
		fopen_s(&file, "test.txt", "w");
		for (uint32_t y = 0; y < path.m_desc.m_size.y; y++)
		{
			std::vector<char> output(path.m_desc.m_size.x + 1);
			for (uint32_t x = 0; x < path.m_desc.m_size.x; x++)
			{
				ivec2 m = ivec2(x, y) >> 3;
				ivec2 c = ivec2(x, y) - (m << 3);
				output[x] = (path.m_field[m.x + m.y*map_size.x] & (1ull << (c.x + c.y * 8))) != 0 ? '#' : ' ';
			}
			output[path.m_desc.m_size.x] = '\0';
			fprintf(file, "%s\n", output.data());
		}
		fclose(file);
	}
	void writeSolvePath(const PathContextCPU& path, const std::vector<uint64_t>& solver, const std::string& filename)
	{
		auto map_size = path.m_desc.m_size >> 3;
		FILE* file;
		fopen_s(&file, filename.c_str(), "w");
		for (uint32_t y = 0; y < path.m_desc.m_size.y; y++)
		{
			std::vector<char> output(path.m_desc.m_size.x + 1);
			for (uint32_t x = 0; x < path.m_desc.m_size.y; x++)
			{
				ivec2 m = ivec2(x, y) >> 3;
				ivec2 c = ivec2(x, y) - (m << 3);
				uint64_t bit = 1ull << (c.x + c.y * 8);
				char input = (path.m_field[m.x + m.y*map_size.x] & bit) != 0 ? '#' : ' ';
				input = (solver[m.x + m.y*map_size.x] & bit) != 0 ? '@' : input;
				output[x] = input;
			}
			output[path.m_desc.m_size.x] = '\0';
			fprintf(file, "%s\n", output.data());
		}
		fclose(file);
	}

};
