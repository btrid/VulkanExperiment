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

enum RT_WAVE_FORMAT
{
	RT_WAVE_FORMAT_PCM = 1,
	RT_WAVE_FORMAT_EXTENSIBLE = 0xFFFE,
};

struct WaveFormat {
	short waveFormatType_;      ///< RT_WAVE_FORMAT
	short channel_;             ///< �X�e���I�H���m�����H
	long samplesPerSec_;        ///< �T���v�����[�g
	long bytesPerSec_;          ///< ?
	short blockSize_;           ///< �u���b�N�T�C�Y(?)
	short bitsPerSample_;       ///<
};

// ���j�APCM�Ƃ��Ȃ�Ƃ�
struct WaveFormatPCM {
	WaveFormat format_;

	/// �_�E���L���X�g�R���X�g���N�^
	WaveFormatPCM(const WaveFormat& rhv)
	{
		format_ = rhv;
	}

	const WaveFormatPCM& operator=(const WaveFormat& rhv)
	{
		format_ = rhv;
		return *this;
	}
};

typedef struct _GUID {
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID;

struct WaveFormatExtensible
{
	short      size_;             /* the count in bytes of the size of */
	union {
		short validBitsPerSample_;       /* bits of precision  */
		short samplesPerBlock_;          /* valid if wBitsPerSample==0 */
		short reserved_;                 /* If neither applies, set to zero. */
	} Samples;
	long        channelMask_;      /* which channels are */
								   /* present in stream  */
	GUID        SubFormat_;
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
	: m_data_ptr(nullptr)
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
			WaveFormat* waveFormat = read<WaveFormat>(m_data, offset);
			frequency_ = waveFormat->samplesPerSec_;
			bps_ = waveFormat->bytesPerSec_;
			blockSize_ = waveFormat->blockSize_;

			if (waveFormat->waveFormatType_ == RT_WAVE_FORMAT_PCM)
			{
//				WaveFormatPCM pcm = waveFormat;
			}
			else if (waveFormat->waveFormatType_ == static_cast<short>(RT_WAVE_FORMAT_EXTENSIBLE))
			{
				WaveFormatExtensible* ext = read<WaveFormatExtensible>(m_data, offset);
				m_size = ext->size_;
				m_samples.m_valid_bits_per_sample = ext->Samples.validBitsPerSample_;
				m_channel_mask = ext->channelMask_;
// 				fread(&ext.SubFormat_, 1, sizeof(ext.SubFormat_), fileHandle_);
//				offset += chunk->chunkSize_ - sizeof(WaveFormat);
			}
			else
			{
				// ����`�̃t�H�[�}�b�g�^�C�v�̏ꍇ�͔�΂�
				offset += chunk->chunkSize_ - sizeof(WaveFormat);
			}

		}
		else if (strncmp(chunk->chunkName_, "data", 4) == 0)
		{
			// ���f�[�^��ǂݍ���
			m_data_ptr = m_data.data() + offset;
			m_data_length = chunk->chunkSize_;
			offset += chunk->chunkSize_;
		}
// 		else if (strncmp(chunk.chunkName_, "cue ", 4) == 0) {
// 			// ������
// 			file.seekg(chunk.chunkSize_ - sizeof(WaveFormat), std::ios::cur);
// 		}
		else {
			// ������
			// fmt��data�ȊO�̃`�����N�̓I�v�V�����Ȃ̂œǂݍ��ޕK�v�͂Ȃ�
			offset += chunk->chunkSize_;
		}
	}

}


