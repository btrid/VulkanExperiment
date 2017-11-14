#pragma once

#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

#include <atlbase.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

struct sSoundSystem : Singleton<sSoundSystem>
{
	friend Singleton<sSoundSystem>;
	void setup(std::shared_ptr<btr::Context>& context);
	void execute_loop(const std::shared_ptr<btr::Context>& context);

	CComPtr<IMMDeviceEnumerator> m_device_enumerator;	// マルチメディアデバイス列挙インターフェース
	CComPtr<IMMDevice>			m_device;				// デバイスインターフェース
	CComPtr<IAudioClient>		m_audio_client;			// オーディオクライアントインターフェース
	CComPtr<IAudioRenderClient>	m_render_client;		// レンダークライアントインターフェース
	WAVEFORMATEXTENSIBLE m_format;
	HANDLE				m_event;

	int						m_current;						// WAVファイルの再生位置
	btr::BufferMemoryEx<int32_t> m_buffer;
};