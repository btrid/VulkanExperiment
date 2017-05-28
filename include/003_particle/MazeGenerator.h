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
		CELL_TYPE_ABS_WALL = -127, ///< 外周	
		CELL_TYPE_PATH = 0,		///< 道
		CELL_TYPE_WALL = 1,		///< 壁
	};

	enum {
		GROUP_X = 16,
		GROUP_Y = 16,
		GROUP_SIZE = GROUP_X*GROUP_Y,
	};

	struct Map{
		int x_;
		int y_;
		std::vector<int> map_;


		int& data(int x, int y){
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
			field_.data(x, 1) = CELL_TYPE_WALL;
			field_.data(x, field_.y_ - 2) = CELL_TYPE_WALL;
		}
		for (int y = 1; y < field_.y_ - 1; y++)
		{
			field_.data(1, y) = CELL_TYPE_WALL;
			field_.data(field_.x_ - 2, y) = CELL_TYPE_WALL;
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

		solver_.x_ = sizex;
		solver_.y_ = sizey;
		solver_.map_.resize(sizex*sizey);
//		solve();
	}
	bool _generateSub(Map& map, short x, short y)
	{
		{
			short		px, py, r, c, p;

			r = std::rand() % 4;		//	ランダムな方向
			p = 1;					//	回転させる向き
			c = 0;					//	方向転換した回数
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

				//	壁にぶつかったら、処理完了で戻す
				if (map.dataSafe(x + px * 2, y + py * 2) == CELL_TYPE_WALL)
				{
					map.data(x, y) = CELL_TYPE_WALL;
					map.data(x + px, y + py) = CELL_TYPE_WALL;
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
						map.data(x, y) = CELL_TYPE_WALL;
						map.data(x + px, y + py) = CELL_TYPE_WALL;
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

	}
	void solve()
	{
		List open;
		std::vector<CloseNode> close(getSizeX() * getSizeY());
//		List close;
		int x = std::rand() % solver_.x_;
		int y = std::rand() % solver_.y_;
		x = x + x % 2 + 1;
		y = y + y % 2 + 1;
		start_ = Node{ x, y, 0, -1 };
		open.push_back(start_);
		const int SIZE_X = field_.x_;
		const int SIZE_Y = field_.y_;
		Node g{ std::rand() % solver_.x_, std::rand() % solver_.y_, std::numeric_limits<int>::max(), -1 };
		while (!open.empty())
		{
			Node n = open[0];
			open.pop_front();

			solver_.data(n.x, n.y) = n.cost;

			if (n.x == g.x && n.y == g.y && n.cost < g.cost)
			{
				// @Todo最短とは限らない
				g = n;
				//				break;
			}

			// 右
			if (n.x < SIZE_X - 1)
			{
				_setNode(n.x + 1, n.y, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
			}
			// 左
			if (n.x > 0)
			{
				_setNode(n.x - 1, n.y, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
			}
			// 下
			if (n.y < SIZE_Y - 1)
			{
				_setNode(n.x, n.y + 1, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
			}
			// 上
			if (n.y > 0)
			{
				_setNode(n.x, n.y - 1, calcNodeIndex(n.x, n.y), n.cost + 1, open, close);
			}
		}

	}
	void _setNode(int x, int y, int parent, int cost, List& open, CloseList& close)
	{

		// 通路ならOK
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
			// close を上に持っていったほうが早いと思う
			auto& cn = close[x+y*getSizeX()];
			if (cn.cost >= 0 && cost > cn.cost) {
				break;
			}
			else{
				cn.cost = cost;
				cn.parent = parent;
				// break;
			}
			open.push_back({ x, y, cost, parent });

		} while (false);

	}	
	void draw(float deltaTime)
	{
	}
	Node start_;
	Node goal_;

	Map field_;
	Map solver_;

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
		std::int32_t isFlush; ///< 行き止まり
		std::int32_t flushing;
		std::int32_t __p[3];
	};
	int index_ = 0;
	void progress(){ index_ = (index_ + 1) % 2; }
	int current()const{ return index_; }
	int next()const{ return (index_ + 1) % 2; }

	float time_ = 0.f;

	Node& getStart(){ return start_; }
	Node& getGoal(){ return goal_; }
	std::vector<int> distance_;


	int calcNodeIndex(int x, int y){
		return y*solver_.x_ + x;
	}

	std::vector<int>& getData(){ return field_.map_; }
	std::vector<int>& getDistance(){ return solver_.map_; }

	int getSizeX()const{ return field_.x_; }
	int getSizeY()const{ return field_.y_; }

	std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> makeGeometry(const glm::vec3& size)const;
	std::vector<uint8_t> makeMapData()const;
};

