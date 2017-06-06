#include <003_particle/MazeGenerator.h>
//#include <random>

std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MazeGenerator::makeGeometry(const glm::vec3& size)const
{
	std::vector<glm::vec3> vertex;
	std::vector<glm::uvec3> index;
	auto& data = field_.map_;
//	glm::vec3 offset;

	// è∞
	auto index_offset = vertex.size();
	vertex.push_back(glm::vec3(0, 0, 0)*size);
	vertex.push_back(glm::vec3(0, 0, getSizeY())*size);
	vertex.push_back(glm::vec3(getSizeX(), 0, 0)*size);
	vertex.push_back(glm::vec3(getSizeX(), 0, getSizeY())*size);
	index.push_back(glm::uvec3(index_offset, index_offset + 1, index_offset + 2));
	index.push_back(glm::uvec3(index_offset + 1, index_offset + 3, index_offset + 2));

	for (size_t y = 0; y < getSizeY(); y++)
	{
		for (size_t x = 0; x < getSizeX(); x++)
		{
			int current = data[y * getSizeX() + x];
			if (current == MazeGenerator::CELL_TYPE_ABS_WALL
				|| current == MazeGenerator::CELL_TYPE_WALL)
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

std::vector<uint8_t> MazeGenerator::makeMapData()const
{
	std::vector<uint8_t> data(getSizeX()*getSizeY());
	for (auto y = 0; y < getSizeX(); y++)
	{
		for (auto x = 0; x < getSizeX(); x++)
		{
			data[x+y*getSizeX()] = (uint8_t)field_.map_[x + y*getSizeX()];
		}
	}
	return data;
}
