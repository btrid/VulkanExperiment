#pragma once

#include <string>
#include <vector>
struct rWave
{
private:
	std::string m_filename;		///< �t�@�C����
	std::vector<char> m_data;   //!< wave�t�@�C���{��
	int channel_;				///< �X�e���I/���m����
	int frequency_;				///< �W�{�����g��
	int bps_;					///< �ʎq�����x
	int blockSize_;				///< �Ԃ�����T�C�Y?

	short      m_size;             /* the count in bytes of the size of */
	union {
		short m_valid_bits_per_sample;       /* bits of precision  */
		short m_samples_per_block;          /* valid if wBitsPerSample==0 */
		short reserved_;                 /* If neither applies, set to zero. */
	} m_samples;
	long        m_channel_mask;      /* which channels are */
								   /* present in stream  */
//	GUID        SubFormat_;

	char* m_data_ptr;				///< �f�[�^�ʒu
	int m_data_length;				///< ���f�[�^�̒���
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