#pragma once

#include <string>
#include <vector>
struct rWave
{
private:
	std::string m_filename;		///< ファイル名
	std::vector<char> m_data;   //!< waveファイル本体
	int channel_;				///< ステレオ/モノラル
	int frequency_;				///< 標本化周波数
	int bps_;					///< 量子化精度
	int blockSize_;				///< ぶろっくサイズ?

	char* m_data_ptr;				///< データ位置
	int m_data_length;				///< 音データの長さ
public:
	int getChannel()const { return channel_; }
	int getfrequency()const { return frequency_; }
	int getBps()const { return bps_; }
	int getBlockSize()const { return blockSize_; }
	//    const char* getData()const { return data_; }

	char* getData()const { return m_data_ptr; }
	int getLength()const { return m_data_length; }

	rWave(const std::string& file);
};