#pragma once
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <array>
#include <glm/glm.hpp>


class rTexture
{
public:
	struct LoadParam
	{
		bool isFlipY;
		LoadParam()
			: isFlipY(false)
		{

		}
	};

	struct Data{
		std::vector<glm::vec4> m_data;
		glm::uvec3 m_size;

		size_t getBufferSize()const { return m_data.size() * sizeof(decltype(m_data)::value_type); }
	};
	static Data LoadTexture(const std::string& file, const LoadParam& param = LoadParam());

};

class Sampler{

};
