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
	uint32_t m_sound_deltatime;
	uint32_t m_write_start;
	uint32_t _p2;
	uint32_t _p3;
	glm::vec4 m_listener;
	glm::vec4 m_direction;

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
	int32_t	m_request_length;
};

struct sSoundSystem : Singleton<sSoundSystem>
{
	friend Singleton<sSoundSystem>;

	void setup(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer execute_loop(const std::shared_ptr<btr::Context>& context);

	void setSoundbank(const std::shared_ptr<btr::Context>& context, std::vector<std::shared_ptr<SoundBuffer>>&& bank);
	std::weak_ptr<SoundPlayRequestData> playOneShot(int32_t play_sound_id);

	const WAVEFORMATEX* getFormat()const { return &m_format.Format; }

private:
	enum 
	{
		SOUND_BANK_SIZE = 1,
		SOUND_REQUEST_SIZE = 1,
		SOUND_BUFFER_FRAME = 3,
	};

	enum Shader
	{
		SHADER_SOUND_PLAY,
		SHADER_SOUND_UPDATE,
		SHADER_SOUND_REQUEST,
		SHADER_NUM,
	};
	enum Pipeline
	{
		PIPELINE_SOUND_PLAY,
		PIPELINE_SOUND_UPDATE,
		PIPELINE_SOUND_REQUEST,
		PIPELINE_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_SOUND_PLAY,
		PIPELINE_LAYOUT_SOUND_UPDATE,
		PIPELINE_LAYOUT_SOUND_REQUEST,
		PIPELINE_LAYOUT_NUM,
	};

	CComPtr<IMMDeviceEnumerator> m_device_enumerator;	// マルチメディアデバイス列挙インターフェース
	CComPtr<IMMDevice>			m_device;				// デバイスインターフェース
	CComPtr<IAudioClient>		m_audio_client;			// オーディオクライアントインターフェース
	CComPtr<IAudioRenderClient>	m_render_client;		// レンダークライアントインターフェース
	WAVEFORMATEXTENSIBLE m_format;

	int32_t						m_current;						// WAVファイルの再生位置

	vk::UniqueDescriptorPool		m_descriptor_pool;
	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout;
	vk::UniqueDescriptorSet			m_descriptor_set;

	std::array<vk::UniqueShaderModule, SHADER_NUM>	m_shader_module;
	std::array<vk::UniquePipeline, PIPELINE_NUM>	m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;

	btr::BufferMemoryEx<SoundFormat> m_sound_format;
	btr::UpdateBuffer<SoundPlayInfo> m_sound_play_info;
	btr::BufferMemoryEx<int32_t> m_buffer;
	btr::BufferMemoryEx<uvec3> m_sound_counter;
	btr::BufferMemoryEx<int32_t> m_active_buffer;
	btr::BufferMemoryEx<int32_t> m_free_buffer;
	btr::BufferMemoryEx<SoundPlayRequestData> m_playing_buffer;
	btr::BufferMemoryEx<SoundPlayRequestData> m_request_buffer;

	std::array<std::shared_ptr<SoundBuffer>, SOUND_BANK_SIZE> m_soundbank;
	std::vector<std::shared_ptr<SoundPlayRequestData>> m_sound_request_data_cpu;
};