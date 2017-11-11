#pragma once

#include <string>
#include <vector>
#include <btrlib/Define.h>
#include <mmreg.h>

struct rWave
{
private:
	std::string m_filename;		///< �t�@�C����
	std::vector<char> m_data;   //!< wave�t�@�C���{��

	WAVEFORMAT* m_wave_format;
	char* m_data_ptr;				///< �f�[�^�ʒu
	int m_data_length;				///< ���f�[�^�̒���
public:

	char* getData()const { return m_data_ptr; }
	int getLength()const { return m_data_length; }

	rWave(const std::string& file);
};