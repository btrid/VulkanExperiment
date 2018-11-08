#pragma once

#include <vector>
#include <random>
#include <cstdint>
#include<fstream>

#include <btrlib/DefineMath.h>
struct TestField
{
	TestField()
	{
//		auto a = 1 << 15;
		auto a = 1 << 9;
		auto aa = a >> 3;
		m_w = a;
		m_h = a;
		m_field.resize(aa*aa);
		for (int y = 0; y < aa; y++)
		{
			for (int x = 0; x < aa; x++)
			{
				uint64_t m = 0;
				for (int i = 0; i < 64; i++)
				{
					m |= glm::simplex(vec2(x * 8 + i % 8, y * 8 + i / 8) / 128.f) >= 0.5f ? (1ull<<i) : 0ull;
				}
				m_field[y*aa + x] = m;
			}
		}
	}

	void write()
	{
		auto wm = m_w >> 3;
		auto hm = m_h >> 3;
		FILE* file;
		fopen_s(&file, "test.txt", "w");
		for (uint32_t y = 0; y < m_h; y++)
		{
			std::vector<char> output(m_h+1);
			for (uint32_t x = 0; x < m_w; x++)
			{
				ivec2 m = ivec2(x, y) >> 3;
				ivec2 c = ivec2(x, y) - (m<<3);
				output[x] = (m_field[m.x + m.y*wm] & (1ull << (c.x + c.y * 8))) != 0 ? '#' : ' ';
			}
			output[m_w] = '\0';
			fprintf(file, "%s\n", output.data());
		}
		fclose(file);
	}
	uint32_t m_w;
	uint32_t m_h;
	std::vector<uint64_t> m_field;
};