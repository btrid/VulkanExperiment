#include <cstdio>
#include <cstdint>
#include <array>
#include <memory>
#include <btrlib/cAudio.h>
#include <btrlib/ResourceManager.h>
#include <ogg/ogg.h>
#include <vorbis/vorbisfile.h>

namespace 
{

// ov_callbacks�̃R�[���o�b�N�Q
size_t ov_read_func(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return fread(ptr, size, nmemb, (FILE*)datasource);
}

int ov_seek_func(void *datasource, ogg_int64_t offset, int whence)
{
	return fseek((FILE*)datasource, (long)offset, whence);
}

int ov_close_func(void *datasource)
{
	return fclose((FILE*)datasource);
}

long ov_tell_func(void *datasource)
{
	return ftell((FILE*)datasource);
}

/**
	*	�r�b�O�G���f�B�A���H
	*/
bool isBigEndian()
{
	int a = 1;
	return !((char*)&a)[0];
}

void Swap(short &s1, short &s2)
{
	short sTemp = s1;
	s1 = s2;
	s2 = sTemp;
}

}


uint32_t DecodeOggVorbis(OggVorbis_File* psOggVorbisFile, char *pDecodeBuffer, unsigned long ulBufferSize, unsigned long ulChannels)
{
	int current_section;

	unsigned long decode_done = 0;
	while (1)
	{
		// ov_read�͑�2�����ɍő��3�����܂ł̃f�[�^��ǂݍ���.
		// ���ۂɓǂݍ��ނ͖̂߂�l���Ȃ̂ŁA�ő�܂œǂݍ��ނ��߉��x���Ăяo���Ă���
		// pDecodeBuffer�Ƀf�[�^�𖄂߂�
		auto decode_size = ov_read(psOggVorbisFile, pDecodeBuffer + decode_done, ulBufferSize - decode_done, isBigEndian(), 2, 1, &current_section);
		if (decode_size > 0)
		{
			decode_done += decode_size;

			if (decode_done >= ulBufferSize)
				break;
		}
		else
		{
			break;
		}
	}

	// Mono, Stereo and 4-Channel files decode into the same channel order as WAVEFORMATEXTENSIBLE,
	// however 6-Channels files need to be re-ordered
	if (ulChannels == 6)
	{
		short* pSamples = (short*)pDecodeBuffer;
		for (unsigned long ulSamples = 0; ulSamples < (ulBufferSize >> 1); ulSamples += 6)
		{
			// WAVEFORMATEXTENSIBLE Order : FL, FR, FC, LFE, RL, RR
			// OggVorbis Order            : FL, FC, FR,  RL, RR, LFE
			Swap(pSamples[ulSamples + 1], pSamples[ulSamples + 2]);
			Swap(pSamples[ulSamples + 3], pSamples[ulSamples + 5]);
			Swap(pSamples[ulSamples + 4], pSamples[ulSamples + 5]);
		}
	}

	return decode_done;
}

class SoundPlayer
{

private:
	struct Resource
	{
		Resource()
			: m_file_handle(nullptr)
			, m_vorbis_Info(NULL)

		{

		}
		enum {
			BUFFER_NUM = 4,
		};
		float		m_time;
		int         m_channels;      //!< ���m�����H�X�e���I�H
		int         m_frequency;     //!< �T���v���^�b�T���v�����O���[�g
		int         blockSize_;     //!< ���f�[�^�̍ŏ��T�C�Y�B�`�����l���� * 1�T���v��������̃r�b�g�� / 8�ŋ��߂���B

		std::string m_filename;
		OggVorbis_File	m_ogg_vorbis_file;
		vorbis_info*    m_vorbis_Info;
		FILE*           m_file_handle;
		float           m_current;            ///< ���Đ����Ă���ꏊ

		short       format_;
		char*       bufferData_;            //!< ���̃f�[�^��ۑ����Ă����ꏊ
		unsigned    bufferSize_;            //!< buffer_�̃T�C�Y
		int         totalBufferProcessed_;  //!< �o�b�t�@�������i�񂾂�
		float       secOffset_;             //!< �Đ�����
		std::array<unsigned, BUFFER_NUM>    buffer_;    ///< �o�b�t�@�[
	};
	std::shared_ptr<Resource>   m_resoure;			        //!< �I�[�f�B�I�̃f�[�^
	bool        isLoop_;
	bool        isFadeout_;

public:
	SoundPlayer(){}
	~SoundPlayer() {}

	void setup(const std::string& filename)
	{
		m_resoure = std::make_shared<Resource>();
		ov_callbacks callbacks;
		callbacks.read_func = ov_read_func;
		callbacks.seek_func = ov_seek_func;
		callbacks.close_func = ov_close_func;
		callbacks.tell_func = ov_tell_func;

		if (ov_open_callbacks(m_resoure->m_file_handle, &m_resoure->m_ogg_vorbis_file, NULL, 0, callbacks) != 0) {
			assert(false);
			return;
		}

		m_resoure->m_vorbis_Info = ov_info(&m_resoure->m_ogg_vorbis_file, -1);
		if (!m_resoure->m_vorbis_Info) {
			::printf("[] is different ogg file. \n");
			assert(false);
			return;
		}
		long ulFrequency = m_resoure->m_vorbis_Info->rate;
		int ulChannels = m_resoure->m_vorbis_Info->channels;
	}

	void uninitialize()
	{
		ov_clear(&m_resoure->m_ogg_vorbis_file);
		m_resoure->m_vorbis_Info = NULL;
	}
// 	int getNextBufferData(char* data, unsigned dataSize)
// 	{
// 		int decodeSize = DecodeOggVorbis(&m_ogg_vorbis_file, data, dataSize, m_vorbis_Info->channels);
// 		currentPos_ = static_cast<float>(ov_time_tell(&m_ogg_vorbis_file));
// 		return decodeSize;
// 	}
// 	void getAudioData(AudioData& data)
// 	{
// 		data.m_channels = vorbisInfo_->channels;
// 		data.m_frequency = vorbisInfo_->rate;
// 		data.m_time = static_cast<float>(ov_time_total(&m_ogg_vorbis_file, -1));
// 	}
// 
// 	float getCurrentTime()const
// 	{
// 		return currentPos_;
// 	}
// 
// 	void seek(float time)
// 	{
// 		ov_time_seek(&m_ogg_vorbis_file, static_cast<float>(time));
// 		currentPos_ = static_cast<float>(ov_time_tell(&m_ogg_vorbis_file));
// 	}
// 
};
