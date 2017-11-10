#pragma once

#include <btrlib/Singleton.h>

#include <atlbase.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

struct sSoundSystem : Singleton<sSoundSystem>
{
	friend Singleton<sSoundSystem>;
	void setup();

	CComPtr<IMMDeviceEnumerator> m_device_enumerator;	// マルチメディアデバイス列挙インターフェース
	CComPtr<IMMDevice>			m_device;				// デバイスインターフェース
	CComPtr<IAudioClient>		m_audio_client;			// オーディオクライアントインターフェース
	CComPtr<IAudioRenderClient>	m_render_client;		// レンダークライアントインターフェース

};