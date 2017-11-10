#include <010_sound/sSoundSystem.h>

#include <string>
#include <cassert>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <FunctionDiscoveryKeys_devpkey.h>

std::string to_string(HRESULT ret)
{	
	switch (ret)
	{
		case AUDCLNT_E_NOT_INITIALIZED:					return "AUDCLNT_E_NOT_INITIALIZED";
		case AUDCLNT_E_ALREADY_INITIALIZED:				return "AUDCLNT_E_ALREADY_INITIALIZED";
		case AUDCLNT_E_WRONG_ENDPOINT_TYPE:				return "AUDCLNT_E_WRONG_ENDPOINT_TYPE";
		case AUDCLNT_E_DEVICE_INVALIDATED:				return "AUDCLNT_E_DEVICE_INVALIDATED";
		case AUDCLNT_E_NOT_STOPPED:						return "AUDCLNT_E_NOT_STOPPED";
		case AUDCLNT_E_BUFFER_TOO_LARGE:				return "AUDCLNT_E_BUFFER_TOO_LARGE";
		case AUDCLNT_E_OUT_OF_ORDER:					return "AUDCLNT_E_OUT_OF_ORDER";
		case AUDCLNT_E_UNSUPPORTED_FORMAT:				return "AUDCLNT_E_UNSUPPORTED_FORMAT";
		case AUDCLNT_E_INVALID_SIZE:					return "AUDCLNT_E_INVALID_SIZE";
		case AUDCLNT_E_DEVICE_IN_USE:					return "AUDCLNT_E_DEVICE_IN_USE";
		case AUDCLNT_E_BUFFER_OPERATION_PENDING:		return "AUDCLNT_E_BUFFER_OPERATION_PENDING";
		case AUDCLNT_E_THREAD_NOT_REGISTERED:			return "AUDCLNT_E_THREAD_NOT_REGISTERED";
//		case AUDCLNT_E_NO_SINGLE_PROCESS:				return  "AUDCLNT_E_NO_SINGLE_PROCESS";
		case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:		return "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
		case AUDCLNT_E_ENDPOINT_CREATE_FAILED:			return "AUDCLNT_E_ENDPOINT_CREATE_FAILED";
		case AUDCLNT_E_SERVICE_NOT_RUNNING:				return "AUDCLNT_E_SERVICE_NOT_RUNNING";
		case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED:		return "AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED";
		case AUDCLNT_E_EXCLUSIVE_MODE_ONLY:				return "AUDCLNT_E_EXCLUSIVE_MODE_ONLY";
		case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:	return "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL";
		case AUDCLNT_E_EVENTHANDLE_NOT_SET:				return "AUDCLNT_E_EVENTHANDLE_NOT_SET";
		case AUDCLNT_E_INCORRECT_BUFFER_SIZE:			return "AUDCLNT_E_INCORRECT_BUFFER_SIZE";
		case AUDCLNT_E_BUFFER_SIZE_ERROR:				return "AUDCLNT_E_BUFFER_SIZE_ERROR";
		case AUDCLNT_E_CPUUSAGE_EXCEEDED:				return "AUDCLNT_E_CPUUSAGE_EXCEEDED";
		default:										return "UNKNOWN";
	}
}

void sSoundSystem::setup()
{
	CoInitialize(NULL);

	HRESULT ret;
	ret = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&m_device_enumerator));
	assert(SUCCEEDED(ret));

	// �f�t�H���g�̃f�o�C�X��I��
	ret = m_device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
	assert(SUCCEEDED(ret));

	// �I�[�f�B�I�N���C�A���g
	ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
	assert(SUCCEEDED(ret));

	// 	// �t�H�[�}�b�g�̍\�z
	ZeroMemory(&m_format, sizeof(m_format));
	m_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
	m_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	m_format.Format.nChannels = 2;
	m_format.Format.nSamplesPerSec = 44100;
	m_format.Format.wBitsPerSample = 16;
	m_format.Format.nBlockAlign = m_format.Format.nChannels * m_format.Format.wBitsPerSample / 8;
	m_format.Format.nAvgBytesPerSec = m_format.Format.nSamplesPerSec * m_format.Format.nBlockAlign;
	m_format.Samples.wValidBitsPerSample = m_format.Format.wBitsPerSample;
	m_format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	m_format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

//	AUDCLNT_SHAREMODE mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
	AUDCLNT_SHAREMODE mode = AUDCLNT_SHAREMODE_SHARED;
	// �t�H�[�}�b�g�̃T�|�[�g�`�F�b�N
	WAVEFORMATEX* closest = nullptr;
	ret = m_audio_client->IsFormatSupported(mode, (WAVEFORMATEX*)&m_format, &closest);
	if (FAILED(ret)) {
		// ���T�|�[�g�̃t�H�[�}�b�g
		assert(SUCCEEDED(ret));
	}
	if (ret == S_FALSE) {
		// �߂��t�H�[�}�b�g�ɕύX
		m_format.Format = *closest;
		m_format.Samples.wValidBitsPerSample = m_format.Format.wBitsPerSample;
	}

	// ���C�e���V�ݒ�
	REFERENCE_TIME default_device_period = 0;
	REFERENCE_TIME minimum_device_period = 0;
	ret = m_audio_client->GetDevicePeriod(&default_device_period, &minimum_device_period);
	printf("�f�t�H���g�f�o�C�X�s���I�h : %I64d (%f�~���b)\n", default_device_period, default_device_period / 10000.0);
	printf("�ŏ��f�o�C�X�s���I�h       : %I64d (%f�~���b)\n", minimum_device_period, minimum_device_period / 10000.0);

	// ������
	UINT32 frame = 0;
	ret = m_audio_client->Initialize(mode,
		AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		default_device_period,				// �f�t�H���g�f�o�C�X�s���I�h�l���Z�b�g
		default_device_period,				// �f�t�H���g�f�o�C�X�s���I�h�l���Z�b�g
		(WAVEFORMATEX*)&m_format.Format,
		NULL);
	if (FAILED(ret)) {
		if (ret == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
//			DEBUG("�o�b�t�@�T�C�Y�A���C�����g�G���[�̂��ߏC������\n");

			// �C����̃t���[�������擾
			ret = m_audio_client->GetBufferSize(&frame);
//			DEBUG("�C����̃t���[����         : %d\n", frame);
			default_device_period = (REFERENCE_TIME)(10000.0 *						// (REFERENCE_TIME(100ns) / ms) *
				1000 *						// (ms / s) *
				frame /						// frames /
				m_format.Format.nSamplesPerSec +	// (frames / s)
				0.5);							// �l�̌ܓ��H
//			DEBUG("�C����̃��C�e���V         : %I64d (%f�~���b)\n", default_device_period, default_device_period / 10000.0);

			// ��x�j�����ăI�[�f�B�I�N���C�A���g���Đ���
//			SAFE_RELEASE(pAudioClient);
			ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
			assert(SUCCEEDED(ret));

			// �Ē���
			ret = m_audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
				AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
				default_device_period,
				default_device_period,
				(WAVEFORMATEX*)&m_format,
				NULL);
		}
		if (FAILED(ret))
		{
			printf(to_string(ret).c_str());
			assert(false);
		}
	}

	// �C�x���g����
	auto hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	// �C�x���g�̃Z�b�g
	ret = m_audio_client->SetEventHandle(hEvent);
	assert(SUCCEEDED(ret));

	// �����_���[�̎擾
	ret = m_audio_client->GetService(IID_PPV_ARGS(&m_render_client));
	assert(SUCCEEDED(ret));

	// WASAPI���擾
	ret = m_audio_client->GetBufferSize(&frame);
	assert(SUCCEEDED(ret));

	UINT32 size = frame * m_format.Format.nBlockAlign;

	// �[���N���A�����ăC�x���g�����Z�b�g
	LPBYTE pData;
	ret = m_render_client->GetBuffer(frame, &pData);
	assert(SUCCEEDED(ret));

	ZeroMemory(pData, size);
	m_render_client->ReleaseBuffer(frame, 0);

}
