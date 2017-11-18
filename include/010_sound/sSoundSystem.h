#pragma once

#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

#include <atlbase.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

#include <010_sound/Sound.h>

struct SoundPlayRequestData
{
	int32_t m_play_sound_id;
	uint32_t m_current;
	uint32_t m_time;
	uint32_t _p3;
	glm::vec4 m_position; //!< w!=0.f ? 指向性
};
struct SoundPlayInfo
{
	glm::vec4 m_listener;
	glm::vec4 m_direction;

	uint32_t play_sound_num;
};

struct SoundFormat
{
	int32_t		nChannels;          /* number of channels (i.e. mono, stereo...) */
	int32_t		nSamplesPerSec;     /* sample rate */
	int32_t		nAvgBytesPerSec;    /* for buffer estimation */
	int32_t		nBlockAlign;        /* block size of data */
	int32_t		wBitsPerSample;     /* number of bits per sample of mono data */
	int32_t		_p1;
	int32_t		_p2;
	int32_t		_p3;

	// buffer format
	int32_t frame_num;
	int32_t samples_per_frame;// m_format.Format.nSamplesPerSec*m_format.Format.nChannels / 60.f
	int32_t	m_buffer_length;
	int32_t		_p23;
};

struct sSoundSystem : Singleton<sSoundSystem>
{
	friend Singleton<sSoundSystem>;
	sSoundSystem() {}
	void setup(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer execute_loop(const std::shared_ptr<btr::Context>& context);

	void setSoundbank(const std::shared_ptr<btr::Context>& context, std::vector<std::shared_ptr<SoundBuffer>>&& bank);
	std::weak_ptr<SoundPlayRequestData> playOneShot(int32_t play_sound_id);

	const WAVEFORMATEX* getFormat()const { return &m_format.Format; }

private:
	enum 
	{
		SOUND_BANK_SIZE = 64,
		SOUND_REQUEST_SIZE = 256,
	};
	CComPtr<IMMDeviceEnumerator> m_device_enumerator;	// マルチメディアデバイス列挙インターフェース
	CComPtr<IMMDevice>			m_device;				// デバイスインターフェース
	CComPtr<IAudioClient>		m_audio_client;			// オーディオクライアントインターフェース
	CComPtr<IAudioRenderClient>	m_render_client;		// レンダークライアントインターフェース
	WAVEFORMATEXTENSIBLE m_format;

	int							m_current;						// WAVファイルの再生位置
	btr::BufferMemoryEx<SoundFormat> m_sound_format;
	btr::UpdateBuffer<SoundPlayInfo> m_sound_play_info;
	btr::BufferMemoryEx<int32_t> m_buffer;
	btr::BufferMemoryEx<int32_t> m_request_buffer_index;
	btr::BufferMemoryEx<int32_t> m_request_buffer_list;
	btr::BufferMemoryEx<SoundPlayRequestData> m_request_buffer;

//	btr::UpdateBuffer<SoundPlayRequestData> m_sound_request_data_buffer;

	vk::UniqueShaderModule m_play_shader;
	vk::UniqueDescriptorPool		m_descriptor_pool;
	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout;
	vk::UniqueDescriptorSet			m_descriptor_set;

	std::array<std::shared_ptr<SoundBuffer>, SOUND_BANK_SIZE> m_soundbank;
	std::vector<std::shared_ptr<SoundPlayRequestData>> m_sound_request_data_cpu;
	SoundPlayInfo m_sound_play_info_cpu;
};