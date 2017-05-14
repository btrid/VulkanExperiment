#include <003_particle/MazeGenerator.h>
//#include <random>

std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MazeGenerator::makeGeometry()
{
	std::vector<glm::vec3> vertex;
	std::vector<glm::uvec3> index;
	auto& data = getData();
	int num = 0;
	printf("make maze geometry\n");
	float size = 1.f;
	for (size_t y = 0; y < getSizeY(); y++)
	{
		for (size_t x = 0; x < getSizeX(); x++)
		{
			int current = data[y * getSizeX() + x];
			{
				auto offset = vertex.size();
				vertex.push_back(glm::vec3(x, current, y)*size);
				vertex.push_back(glm::vec3(x, current, y + 1)*size);
				vertex.push_back(glm::vec3(x + 1, current, y)*size);
				vertex.push_back(glm::vec3(x + 1, current, y + 1)*size);
				index.push_back(glm::uvec3(offset, offset + 1, offset + 2));
				index.push_back(glm::uvec3(offset + 1, offset + 3, offset + 2));
			}

			int right = field_.dataSafe(x + 1, y);
			int under = field_.dataSafe(x, y + 1);

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
					vertex.push_back(glm::vec3(x+1, 0, y)*size);
					vertex.push_back(glm::vec3(x+1, 0, y + 1)*size);
					vertex.push_back(glm::vec3(x+1, 1, y)*size);
					vertex.push_back(glm::vec3(x+1, 1, y + 1)*size);
					index.push_back(glm::uvec3(offset, offset + 1, offset + 2));
					index.push_back(glm::uvec3(offset + 1, offset + 3, offset + 2));
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
					vertex.push_back(glm::vec3(x, 0, y+1)*size);
					vertex.push_back(glm::vec3(x, 1, y+1)*size);
					vertex.push_back(glm::vec3(x+1, 0, y+1)*size);
					vertex.push_back(glm::vec3(x+1, 1, y+1)*size);
					index.push_back(glm::uvec3(offset, offset + 1, offset + 2));
					index.push_back(glm::uvec3(offset + 1, offset + 3, offset + 2));
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
					vertex.push_back(glm::vec3(x+1, 0, y)*size);
					vertex.push_back(glm::vec3(x+1, 0, y + 1)*size);
					vertex.push_back(glm::vec3(x+1, 1, y)*size);
					vertex.push_back(glm::vec3(x+1, 1, y + 1)*size);
					index.push_back(glm::uvec3(offset, offset + 1, offset + 2));
					index.push_back(glm::uvec3(offset + 1, offset + 3, offset + 2));
				}
				break;
				}
				switch (under)
				{
				case MazeGenerator::CELL_TYPE_ABS_WALL:
				case MazeGenerator::CELL_TYPE_WALL:
				{
					auto offset = vertex.size();
					vertex.push_back(glm::vec3(x, 0, y+1)*size);
					vertex.push_back(glm::vec3(x, 1, y+1)*size);
					vertex.push_back(glm::vec3(x + 1, 0, y+1)*size);
					vertex.push_back(glm::vec3(x + 1, 1, y+1)*size);
					index.push_back(glm::uvec3(offset, offset + 1, offset + 2));
					index.push_back(glm::uvec3(offset + 1, offset + 3, offset + 2));
				}
				break;
				}
			}
			}
		}
	}
	
	return std::tie(vertex, index);
}

