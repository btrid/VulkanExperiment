#include <003_particle/MazeGenerator.h>
//#include <random>

std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MazeGenerator::makeGeometry()
{
	std::vector<glm::vec3> vertex;
	std::vector<glm::uvec3> index;
	auto& data = getData();
	int num = 0;
	for (size_t y = 0; y < getSizeY(); y++)
	{
		for (size_t x = 0; x < getSizeX(); x++)
		{
			int current = data[y * getSizeX() + x];
			{
				auto offset = vertex.size();
				vertex.push_back(glm::vec3(x, current, y));
				vertex.push_back(glm::vec3(x, current, y + 1));
				vertex.push_back(glm::vec3(x + 1, current, y));
				vertex.push_back(glm::vec3(x + 1, current, y + 1));
				index.push_back(glm::uvec3(offset * 4, offset * 4 + 1, offset * 4 + 2));
				index.push_back(glm::uvec3(offset * 4 + 1, offset * 4 + 3, offset * 4 + 2));
			}

			int right = field_.dataSafe(y, + x + 1);
			int under = field_.dataSafe(y + 1, x);

			switch (current)
			{
			case MazeGenerator::CELL_TYPE_ABS_WALL:
			case MazeGenerator::CELL_TYPE_WALL:
			{
				switch (right)
				{
				case MazeGenerator::CELL_TYPE_ABS_WALL:
				case MazeGenerator::CELL_TYPE_WALL:
					break;
				default:
				{
					auto offset = vertex.size();
					vertex.push_back(glm::vec3(x, current, y));
					vertex.push_back(glm::vec3(x, current, y + 1));
					vertex.push_back(glm::vec3(x, current + 1, y));
					vertex.push_back(glm::vec3(x, current + 1, y + 1));
					index.push_back(glm::uvec3(offset * 4, offset * 4 + 1, offset * 4 + 2));
					index.push_back(glm::uvec3(offset * 4 + 1, offset * 4 + 3, offset * 4 + 2));
				}
				break;
				}
				switch (under)
				{
				case MazeGenerator::CELL_TYPE_ABS_WALL:
				case MazeGenerator::CELL_TYPE_WALL:
					break;
				default:
				{
					auto offset = vertex.size();
					vertex.push_back(glm::vec3(x, current, y));
					vertex.push_back(glm::vec3(x, current+1, y));
					vertex.push_back(glm::vec3(x+1, current, y));
					vertex.push_back(glm::vec3(x+1, current+1, y));
					index.push_back(glm::uvec3(offset * 4, offset * 4 + 1, offset * 4 + 2));
					index.push_back(glm::uvec3(offset * 4 + 1, offset * 4 + 3, offset * 4 + 2));
				}
				break;
				}
			}
			break;
			default:
			{
				switch (right)
				{
				case MazeGenerator::CELL_TYPE_ABS_WALL:
				case MazeGenerator::CELL_TYPE_WALL:
				{
					auto offset = vertex.size();
					vertex.push_back(glm::vec3(x, current, y));
					vertex.push_back(glm::vec3(x, current, y + 1));
					vertex.push_back(glm::vec3(x, current + 1, y));
					vertex.push_back(glm::vec3(x, current + 1, y + 1));
					index.push_back(glm::uvec3(offset * 4, offset * 4 + 1, offset * 4 + 2));
					index.push_back(glm::uvec3(offset * 4 + 1, offset * 4 + 3, offset * 4 + 2));
				}
				break;
				}
				switch (under)
				{
				case MazeGenerator::CELL_TYPE_ABS_WALL:
				case MazeGenerator::CELL_TYPE_WALL:
				{
					auto offset = vertex.size();
					vertex.push_back(glm::vec3(x, current, y));
					vertex.push_back(glm::vec3(x, current + 1, y));
					vertex.push_back(glm::vec3(x + 1, current, y));
					vertex.push_back(glm::vec3(x + 1, current + 1, y));
					index.push_back(glm::uvec3(offset * 4, offset * 4 + 1, offset * 4 + 2));
					index.push_back(glm::uvec3(offset * 4 + 1, offset * 4 + 3, offset * 4 + 2));
				}
				break;
				}
			}
			}
		}
	}
	return std::tie(vertex, index);
}

