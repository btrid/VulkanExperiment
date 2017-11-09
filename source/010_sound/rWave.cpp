#include <010_sound/rWave.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <fstream>
#include <filesystem>
struct RiffHeader
{
	char			riff_[4]; // "RIFF"
	unsigned long	riffSize_; // ファイルサイズ-8
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
	short channel_;             ///< ステレオ？モノラル？
	long samplesPerSec_;        ///< サンプルレート
	long bytesPerSec_;          ///< ?
	short blockSize_;           ///< ブロックサイズ(?)
	short bitsPerSample_;       ///<
};

// リニアPCMとかなんとか
struct WaveFormatPCM {
	WaveFormat format_;

	/// ダウンキャストコンストラクタ
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

#ifndef GUID_DEFINED
#define GUID_DEFINED
#if defined(__midl)
typedef struct {
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	byte           Data4[8];
} GUID;
#else
typedef struct _GUID {
	unsigned long  Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char  Data4[8];
} GUID;
#endif
#endif

struct WaveFormatExtensible
{
	WaveFormat format_;
	short      size_;             /* the count in bytes of the size of */

	union {
		short validBitsPerSample_;       /* bits of precision  */
		short samplesPerBlock_;          /* valid if wBitsPerSample==0 */
		short reserved_;                 /* If neither applies, set to zero. */
	} Samples;
	long        channelMask_;      /* which channels are */
								   /* present in stream  */
	GUID        SubFormat_;

	/// ダウンキャストコンストラクタ
	WaveFormatExtensible(const WaveFormat& rhv)
	{
		format_ = rhv;
	}

	const WaveFormatExtensible& operator=(const WaveFormat& rhv)
	{
		format_ = rhv;
		return *this;
	}

};

template<typename T>
T* read(char*& p)
{
	T* pp = (RiffHeader*)(p);
	p += sizeof(T);
	return pp;
}
rWave::rWave(const std::string& filename)
	: m_data_ptr(nullptr)
	, m_data_length(0)
{
//	m_data = filename;
	m_filename = filename;

	// ファイルを開く
//	{
		std::experimental::filesystem::path filepath(filename);
		std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
		assert(file.is_open());

		size_t file_size = (size_t)file.tellg();
		file.clear();
		file.seekg(0);

		m_data.resize(file_size);
		file.read(m_data.data(), m_data.size());
//	}

//	char* p = m_data.data();

//	RiffHeader* header = read<RiffHeader>(p);
	file.clear();
	file.seekg(0);
	RiffHeader header;
	file.read((char*)&header, 4);
	if (header.riff_[0] != 'R'
		|| header.riff_[1] != 'I'
		|| header.riff_[2] != 'F'
		|| header.riff_[3] != 'F')
	{
		::printf("[%s] is not wave\n", filename.c_str());
		assert(false);
		return;
	}

//	fread(&header->riffSize_, 4, 1, fileHandle_);
//	fread(header->wave_, 1, 4, fileHandle_);
	file.read((char*)&header.riffSize_, 4);
	file.read((char*)&header.wave_, 4);
	RiffChunk chunk;
	while (file.read((char*)&chunk, sizeof(RiffChunk)))
	{
		if (strncmp(chunk.chunkName_, "fmt ", 4) == 0)
		{
			// 音データのフォーマットを読み込む
			WaveFormat waveFormat;
// 			fread(&waveFormat.waveFormatType_, 2, 1, fileHandle_);
// 			fread(&waveFormat.channel_, 2, 1, fileHandle_);
// 			fread(&waveFormat.samplesPerSec_, 4, 1, fileHandle_);
// 			fread(&waveFormat.bytesPerSec_, 4, 1, fileHandle_);
// 			fread(&waveFormat.blockSize_, 2, 1, fileHandle_);
// 			fread(&waveFormat.bitsPerSample_, 2, 1, fileHandle_);
			file.read((char*)&waveFormat, sizeof(waveFormat));

			frequency_ = waveFormat.samplesPerSec_;
			bps_ = waveFormat.bytesPerSec_;
			blockSize_ = waveFormat.blockSize_;

			if (waveFormat.waveFormatType_ == RT_WAVE_FORMAT_PCM)
			{
				WaveFormatPCM pcm = waveFormat;
			}
			else if (waveFormat.waveFormatType_ == static_cast<short>(RT_WAVE_FORMAT_EXTENSIBLE))
			{
				WaveFormatExtensible ext = waveFormat;
// 				fread(&ext.size_, 1, sizeof(ext.size_), fileHandle_);
// 				fread(&ext.Samples, 1, sizeof(ext.Samples), fileHandle_);
// 				fread(&ext.channelMask_, 1, sizeof(ext.channelMask_), fileHandle_);
// 				fread(&ext.SubFormat_, 1, sizeof(ext.SubFormat_), fileHandle_);
//				file.read()
				file.seekg(chunk.chunkSize_ - sizeof(WaveFormat), std::ios::cur);
			}
			else
			{
				// 未定義のフォーマットタイプの場合は飛ばす
				file.seekg(chunk.chunkSize_ - sizeof(WaveFormat), std::ios::cur);
//				fseek(fileHandle_, chunk.chunkSize_ - sizeof(WaveFormat), SEEK_CUR);
			}

		}
		else if (strncmp(chunk.chunkName_, "data", 4) == 0)
		{
			// 音データを読み込む
//			length_ = chunk.chunkSize_;
//			data_ = new char[length_];
// 			for (int i = 0; i < length_; i++)
// 			{
// 				char a;
// 				fread(&a, 1, 1, fileHandle_);
// 				data_[i] = a;
// 			}
			m_data_ptr = m_data.data() + file.tellg();
			m_data_length = chunk.chunkSize_;
		}
		else if (strncmp(chunk.chunkName_, "cue ", 4) == 0) {
			// 未実装
			file.seekg(chunk.chunkSize_ - sizeof(WaveFormat), std::ios::cur);
		}
		else {
			// 未実装
			// fmtとdata以外のチャンクはオプションなので読み込む必要はない
			file.seekg(chunk.chunkSize_ - sizeof(WaveFormat), std::ios::cur);
		}
	}

}


