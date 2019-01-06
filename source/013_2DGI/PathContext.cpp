#include <013_2DGI/PathContext.h>

enum CELL_TYPE
{
	CELL_TYPE_PRE_WALL = -128,
	CELL_TYPE_ABS_WALL = -127, ///< 外周	
	CELL_TYPE_PATH = 0,		///< 道
	CELL_TYPE_WALL = 1,		///< 壁
};

struct Map
{
	int32_t m_x;
	int32_t m_y;
	std::vector<char> m_data;
	char& data(int x, int y)
	{
		assert(x >= 0 && x < m_x);
		assert(y >= 0 && y < m_y);
		return m_data[y*m_x + x];
	}
	const char& data(int x, int y)const
	{
		assert(x >= 0 && x < m_x);
		assert(y >= 0 && y < m_y);
		return m_data[y*m_x + x];
	}

	char dataSafe(int x, int y)const
	{
		if (x < 0 || x >= m_x
			|| y < 0 || y >= m_y) {
			return -1;
		}
		return m_data[y*m_x + x];
	}
};

bool _generateSub(Map& map, int32_t x, int32_t y)
{
	int32_t px, py, r, c, p;

	r = std::rand() % 4;		//	ランダムな方向
	p = 1;					//	回転させる向き
	c = 0;					//	方向転換した回数
	while (c++ < 4)
	{
		switch ((r + c * p + 4) % 4)
		{
		case 0:
			px = 0;
			py = -1;
			break;
		case 1:
			px = -1;
			py = 0;
			break;
		case 2:
			px = 0;
			py = 1;
			break;
		case 3:
			px = 1;
			py = 0;
			break;
		}

		//	壁にぶつかったら、処理完了で戻す
		if (map.dataSafe(x + px * 2, y + py * 2) >= 1)
		{
			map.data(x, y) = CELL_TYPE_WALL * (std::rand() % 5 + 1);
			map.data(x + px, y + py) = CELL_TYPE_WALL * (std::rand() % 5 + 1);
			return true;
		}

		//	行き先が道なら、その方向に「作りかけの壁」を延ばしていく
		if (map.dataSafe(x + px * 2, y + py * 2) == CELL_TYPE_PATH)
		{
			map.data(x, y) = CELL_TYPE_PRE_WALL;
			map.data(x + px, y + py) = CELL_TYPE_PRE_WALL;

			if (_generateSub(map, x + px * 2, y + py * 2))
			{
				//	処理完了で戻ってきたら、「作りかけの壁」を「完成した壁」に変える
				map.data(x, y) = CELL_TYPE_WALL * (std::rand() % 5 + 1);
				map.data(x + px, y + py) = CELL_TYPE_WALL * (std::rand() % 5 + 1);
				return true;

			}
			else
			{
				//	エラーで戻ってきたら、道に戻す
				map.data(x, y) = CELL_TYPE_PATH;
				map.data(x + px, y + py) = CELL_TYPE_PATH;
				return false;
			}
		}
	}
	//	４方向とも「作りかけの壁」なら、エラーで戻る
	return false;
}

std::vector<char> pathmake_maze_(int sizex, int sizey)
{
	Map map;
	map.m_x = sizex;
	map.m_y = sizey;
	map.m_data.resize(sizex*sizey);

	for (int x = 1; x < map.m_x - 1; x++)
	{
		map.data(x, 0) = CELL_TYPE_WALL;
		map.data(x, 1) = CELL_TYPE_WALL;
		map.data(x, map.m_y - 2) = CELL_TYPE_WALL;
		map.data(x, map.m_y - 1) = CELL_TYPE_WALL;
	}
	for (int y = 1; y < map.m_y - 1; y++)
	{
		map.data(0, y) = CELL_TYPE_WALL;
		map.data(1, y) = CELL_TYPE_WALL;
		map.data(map.m_x - 2, y) = CELL_TYPE_WALL;
		map.data(map.m_x - 1, y) = CELL_TYPE_WALL;
	}

	for (int y = 3; y < map.m_y - 3; y += 2)
	{
		for (int x = 3; x < map.m_x - 3; x += 2)
		{
			if (map.dataSafe(x, y) == CELL_TYPE_PATH)
			{
				while (!_generateSub(map, x, y)) { ; }
			}
		}
	}

	return map.m_data;
}

std::vector<uint64_t> pathmake_maze(int sizex, int sizey)
{
	auto map = pathmake_maze_(sizex, sizey);
	ivec2 aa = ivec2(sizex, sizey) >> 3;
	std::vector<uint64_t> data(aa.x*aa.y);
	for (int y = 0; y < aa.y; y++)
	{
		for (int x = 0; x < aa.x; x++)
		{
			uint64_t m = 0;
			for (int i = 0; i < 64; i++)
			{
				m |= map[(x * 8 + i % 8) + (y * 8 + i / 8)*sizex] != 0 ? (1ull << i) : 0;
			}
			auto i = y * aa.x + x;
			data[i] = m;
		}
	}

	return data;

}
std::vector<uint64_t> pathmake_noise(int sizex, int sizey)
{
	auto aa = ivec2(sizex, sizey) >> 3;
	std::vector<uint64_t> data(aa.x*aa.y);
	for (int y = 0; y < aa.y; y++)
	{
		for (int x = 0; x < aa.x; x++)
		{
			uint64_t m = 0;
			for (int i = 0; i < 64; i++)
			{
//				m |= glm::simplex(vec2(x * 8 + i % 8, y * 8 + i / 8) / 256.f) >= abs(0.5f) ? (1ull << i) : 0ull;
			}
			data[y*aa.x + x] = m;
		}
	}
	return data;

}
