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

	CComPtr<IMMDeviceEnumerator> m_device_enumerator;	// �}���`���f�B�A�f�o�C�X�񋓃C���^�[�t�F�[�X
	CComPtr<IMMDevice>			m_device;				// �f�o�C�X�C���^�[�t�F�[�X
	CComPtr<IAudioClient>		m_audio_client;			// �I�[�f�B�I�N���C�A���g�C���^�[�t�F�[�X
	CComPtr<IAudioRenderClient>	m_render_client;		// �����_�[�N���C�A���g�C���^�[�t�F�[�X
	WAVEFORMATEXTENSIBLE m_format;
	HANDLE				m_event;

	int						m_current;						// WAV�t�@�C���̍Đ��ʒu
	btr::BufferMemoryEx<int32_t> m_buffer;
};