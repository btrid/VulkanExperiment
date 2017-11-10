#include <010_sound/rWave.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <fstream>
#include <filesystem>

struct RiffHeader
{
	char			riff_[4]; // "RIFF"
	unsigned long	riffSize_; // �t�@�C���T�C�Y-8
	char			wave_[4]; // "WAVE"
};

struct RiffChunk
{
	char			chunkName_[4];
	unsigned long	chunkSize_;
};


template<typename T>
T* read(std::vector<char>& p, uint32_t& offset)
{
	if (offset >= p.size())
	{
		return nullptr;
	}
	T* pp = (T*)(p.data()+ offset);
	offset += sizeof(T);
	return pp;
}
rWave::rWave(const std::string& filename)
	: m_wave_format(nullptr)
	, m_data_ptr(nullptr)
	, m_data_length(0)
{
//	m_data = filename;
	m_filename = filename;

	// �t�@�C�����J��
	{
		std::experimental::filesystem::path filepath(filename);
		std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
		assert(file.is_open());

		size_t file_size = (size_t)file.tellg();
		file.clear();
		file.seekg(0);

		m_data.resize(file_size);
		file.read(m_data.data(), m_data.size());
	}

	uint32_t offset = 0;
	RiffHeader* header = read<RiffHeader>(m_data, offset);
	if (header->riff_[0] != 'R'
		|| header->riff_[1] != 'I'
		|| header->riff_[2] != 'F'
		|| header->riff_[3] != 'F')
	{
		::printf("[%s] is not wave\n", filename.c_str());
		assert(false);
		return;
	}

	while (RiffChunk* chunk = read<RiffChunk>(m_data, offset))
	{
		if (strncmp(chunk->chunkName_, "fmt ", 4) == 0)
		{
			// ���f�[�^�̃t�H�[�}�b�g��ǂݍ���
			m_wave_format = read<WAVEFORMAT>(m_data, offset);
			offset += chunk->chunkSize_ - sizeof(WAVEFORMAT);
		}
		else if (strncmp(chunk->chunkName_, "data", 4) == 0)
		{
			// ���f�[�^��ǂݍ���
			m_data_ptr = m_data.data() + offset;
			m_data_length = chunk->chunkSize_;
			offset += chunk->chunkSize_;
		}
		else {
			// fmt��data�ȊO�̃`�����N�̓I�v�V�����Ȃ̂œǂݍ��ޕK�v�͂Ȃ�
			offset += chunk->chunkSize_;
		}
	}

}


