#pragma once
#include <vector>
#include <algorithm>
#include <deque>
#include <btrlib/Define.h>

class MazeGenerator
{
public:
	enum CELL_TYPE
	{
		CELL_TYPE_PRE_WALL = -128,
		CELL_TYPE_ABS_WALL = -127, ///< �O��	
		CELL_TYPE_PATH = 0,		///< ��
		CELL_TYPE_WALL = 1,		///< ��
	};

	struct Map{
		int x_;
		int y_;
		std::vector<int> map_;


		int& data(int x, int y) {
			assert(x >= 0 && x < x_);
			assert(y >= 0 && y < y_);
			return map_[y*x_ + x];
		}
		const int& data(int x, int y)const {
			assert(x >= 0 && x < x_);
			assert(y >= 0 && y < y_);
			return map_[y*x_ + x];
		}

		int dataSafe(int x, int y)const{
			if (x<0 || x >= x_
				|| y<0 || y >= y_) {
				return -1;
			}
			return map_[y*x_ + x];
		}


	};
	typedef std::vector<CELL_TYPE> Maze;

public:

	struct Node{
		int x, y, cost, parent;
	};
	using List = std::deque< Node >;
	struct CloseNode{
		int cost, parent;
//		int cost = 0, parent = -1;
		CloseNode() 
			: cost(-1)
			, parent(-1){}
	};
	using CloseList = std::vector< CloseNode >;
	void generate(int sizex, int sizey)
	{

		field_.x_ = sizex;
		field_.y_ = sizey;
		field_.map_.resize(sizex*sizey);

		for (int x = 1; x < field_.x_ - 1; x++)
		{
			field_.data(x, 0) = CELL_TYPE_WALL;
			field_.data(x, 1) = CELL_TYPE_WALL;
			field_.data(x, field_.y_ - 2) = CELL_TYPE_WALL;
			field_.data(x, field_.y_ - 1) = CELL_TYPE_WALL;
		}
		for (int y = 1; y < field_.y_ - 1; y++)
		{
			field_.data(0, y) = CELL_TYPE_WALL;
			field_.data(1, y) = CELL_TYPE_WALL;
			field_.data(field_.x_ - 2, y) = CELL_TYPE_WALL;
			field_.data(field_.x_ - 1, y) = CELL_TYPE_WALL;
		}

		for (int y = 3; y < field_.y_ - 3; y += 2)
		{
			for (int x = 3; x < field_.x_ - 3; x += 2)
			{
				if (field_.dataSafe(x, y) == CELL_TYPE_PATH)
				{
					while (!_generateSub(field_, x, y)) { ; }
				}
			}
		}

	}
	bool _generateSub(Map& map, short x, short y)
	{
		{
			short		px, py, r, c, p;

			r = std::rand() % 4;		//	�����_���ȕ���
			p = 1;					//	��]���������
			c = 0;					//	�����]��������
			while (c++ < 4)
			{
				switch ((r + c*p + 4) % 4)
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

				//	�ǂɂԂ�������A���������Ŗ߂�
				if (map.dataSafe(x + px * 2, y + py * 2) >= 1)
				{
					map.data(x, y) = CELL_TYPE_WALL * (std::rand() % 5 + 1);
					map.data(x + px, y + py) = CELL_TYPE_WALL * (std::rand() % 5 + 1);;
					return true;
				}

				//	�s���悪���Ȃ�A���̕����Ɂu��肩���̕ǁv�����΂��Ă���
				if (map.dataSafe(x + px * 2, y + py * 2) == CELL_TYPE_PATH)
				{
					map.data(x, y) = CELL_TYPE_PRE_WALL;
					map.data(x + px, y + py) = CELL_TYPE_PRE_WALL;

					if (_generateSub(map, x + px * 2, y + py * 2))
					{
						//	���������Ŗ߂��Ă�����A�u��肩���̕ǁv���u���������ǁv�ɕς���
						map.data(x, y) = CELL_TYPE_WALL * (std::rand() % 5 + 1);
						map.data(x + px, y + py) = CELL_TYPE_WALL * (std::rand() % 5 + 1);
						return true;

					}
					else
					{
						//	�G���[�Ŗ߂��Ă�����A���ɖ߂�
						map.data(x, y) = CELL_TYPE_PATH;
						map.data(x + px, y + py) = CELL_TYPE_PATH;
						return false;
					}
				}
			}
			//	�S�����Ƃ��u��肩���̕ǁv�Ȃ�A�G���[�Ŗ߂�
			return false;

		}

	}
	std::vector<uint32_t> solveEx(int sx, int sy)const
	{
		const int SIZE_X = field_.x_;
		const int SIZE_Y = field_.y_;
		glm::uvec2 size(SIZE_X, SIZE_Y);
		Node start = Node{ sx, sy, 0, -1 };
		List open;
		open.push_back(start);
		std::vector<CloseNode> close(SIZE_X * SIZE_Y);

		std::vector<uint32_t> result(SIZE_X*SIZE_Y, -1);
		while (!open.empty())
		{
			Node n = open[0];
			open.pop_front();

			result[n.x + n.y * SIZE_X] = n.cost;

			bool is_left_end = n.x == 0;
			bool is_right_end = n.x == SIZE_X;
			bool is_donw_end = n.y == 0;
			bool is_up_end = n.y == SIZE_Y;
			// �E
			if (!is_right_end)
			{
				_setNode(n.x + 1, n.y, calcNodeIndex(n.x, n.y, size), n.cost + 1, open, close);
			}
			// ��
			if (n.x > 0)
			{
				_setNode(n.x - 1, n.y, calcNodeIndex(n.x, n.y, size), n.cost + 1, open, close);
			}
			// ��
			if (n.y < SIZE_Y - 1)
			{
				_setNode(n.x, n.y + 1, calcNodeIndex(n.x, n.y, size), n.cost + 1, open, close);
			}
			// ��
			if (n.y > 0)
			{
				_setNode(n.x, n.y - 1, calcNodeIndex(n.x, n.y, size), n.cost + 1, open, close);
			}
// 			// ����
// 			if (n.x > 0 && n.y > 0)
// 			{
// 				_setNode(n.x - 1, n.y - 1, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
// 			}
// 			// ����
// 			if (n.x > 0 && n.y < SIZE_Y - 1)
// 			{
// 				_setNode(n.x - 1, n.y + 1, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
// 			}
// 			// �E��
// 			if (n.x < SIZE_X - 1 && n.y < SIZE_Y - 1)
// 			{
// 				_setNode(n.x + 1, n.y-1, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
// 			}
// 			// �E��
// 			if (n.x < SIZE_X - 1 && n.y > 0)
// 			{
// 				_setNode(n.x+1, n.y +1, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
// 			}
		}

		return std::move(result);
	}
	void _setNode(int x, int y, int parent, int cost, List& open, CloseList& close)const
	{

		// �ʘH�Ȃ�OK
		if (field_.data(x, y) != CELL_TYPE_PATH)
		{
			return;
		}

		do
		{
			auto& it = std::find_if(open.begin(), open.end(), [x, y](const Node& n){ return x == n.x && y == n.y; });
			if (it != open.end()) {
				if (cost >= it->cost) {
					break;
				}
				else
				{
					it->cost = cost;
					it->parent = parent;
					break;
				}
			}
			// close ����Ɏ����Ă������ق��������Ǝv��
			auto& cn = close[x+y*getSizeX()];
			if (cn.cost >= 0 && cost > cn.cost) {
				break;
			}
			else
			{
				// �����Ƒ����A�N�Z�X������@�����������̂ōX�V
				cn.cost = cost;
				cn.parent = parent;
			}
			open.push_back({ x, y, cost, parent });

		} while (false);

	}	
	int calcNodeIndex(int x, int y, const glm::uvec2& size)const {
		return y*size.x + x;
	}


	Map field_;

	enum DIR : std::int32_t {
		DIR_NON,
		DIR_UP,
		DIR_DOWN,
		DIR_RIGHT,
		DIR_LEFT,
	};
	enum {
		FlushTime = 4,
	};
	struct Param{
		std::uint32_t color;
		float dist;
		std::int32_t isWall;
		std::int32_t isFlush; ///< �s���~�܂�
		std::int32_t flushing;
		std::int32_t __p[3];
	};
	int index_ = 0;
	void progress(){ index_ = (index_ + 1) % 2; }
	int current()const{ return index_; }
	int next()const{ return (index_ + 1) % 2; }

	std::vector<int>& getData(){ return field_.map_; }
	int getSizeX()const{ return field_.x_; }
	int getSizeY()const{ return field_.y_; }

	std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> makeGeometry(const glm::vec3& size)const;
	std::vector<uint8_t> makeMapData()const;
};

