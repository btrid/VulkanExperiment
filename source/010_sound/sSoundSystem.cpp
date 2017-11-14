#include <010_sound/sSoundSystem.h>

#include <string>
#include <cassert>
#include <chrono>
#include <thread>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <FunctionDiscoveryKeys_devpkey.h>

#include <010_sound/rWave.h>

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
//		case AUDCLNT_E_NO_SINGLE_PROCESS:				return "AUDCLNT_E_NO_SINGLE_PROCESS";
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
		case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:			return "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";
		default:										return "UNKNOWN";
	}
}

bool succeeded(HRESULT result)
{
	if (SUCCEEDED(result))
	{
		return true;
	}

	printf("%s\n", to_string(result).c_str());
	return false;
}
void sSoundSystem::setup(std::shared_ptr<btr::Context>& context)
{
	CoInitialize(NULL);

	HRESULT ret;
	ret = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&m_device_enumerator));
	assert(succeeded(ret));

	// �f�t�H���g�̃f�o�C�X��I��
	ret = m_device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
	assert(succeeded(ret));

	// �I�[�f�B�I�N���C�A���g
	ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
	assert(succeeded(ret));

	// �t�H�[�}�b�g�̍\�z
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
//#define USE_EXCLUSIVE
#ifdef USE_EXCLUSIVE
	AUDCLNT_SHAREMODE share_mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
	int stream_flag = AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
#else
	AUDCLNT_SHAREMODE share_mode = AUDCLNT_SHAREMODE_SHARED;
	int stream_flag = AUDCLNT_STREAMFLAGS_NOPERSIST; // ���L���[�h��EventDriven�����Ȃ��炵��
#endif
	// �t�H�[�}�b�g�̃T�|�[�g�`�F�b�N
	WAVEFORMATEX* closest = nullptr;
	ret = m_audio_client->IsFormatSupported(share_mode, (WAVEFORMATEX*)&m_format, &closest);
	if (FAILED(ret)) {
		// ���T�|�[�g�̃t�H�[�}�b�g
		assert(succeeded(ret));
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
	ret = m_audio_client->Initialize(share_mode,
		stream_flag,
		default_device_period,				// �f�t�H���g�f�o�C�X�s���I�h�l���Z�b�g
		default_device_period,				// �f�t�H���g�f�o�C�X�s���I�h�l���Z�b�g
		(WAVEFORMATEX*)&m_format,
		NULL);

	if (FAILED(ret)) {
		if (ret == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
			//	�o�b�t�@�T�C�Y�A���C�����g�G���[�̂��ߏC������
			UINT32 frame = 0;
			ret = m_audio_client->GetBufferSize(&frame);
			default_device_period =
				(REFERENCE_TIME)(10000.0 *			// (REFERENCE_TIME(100ns) / ms)
					1000 *								// (ms / s) *
					frame /								// frames /
					m_format.Format.nSamplesPerSec +	// (frames / s)
					0.5);								// �l�̌ܓ��H

														// �I�[�f�B�I�N���C�A���g���Đ���
			m_audio_client.Release();
			ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
			assert(succeeded(ret));

			// �Ē���
			ret = m_audio_client->Initialize(share_mode,
				stream_flag,
				default_device_period,
				default_device_period,
				(WAVEFORMATEX*)&m_format,
				NULL);
		}
		assert(succeeded(ret));
	}

	m_event = nullptr;
	if (stream_flag & AUDCLNT_STREAMFLAGS_EVENTCALLBACK)
	{
		// �C�x���g����
		m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		// �C�x���g�̃Z�b�g
		ret = m_audio_client->SetEventHandle(m_event);
		assert(succeeded(ret));

	}

	// �����_���[�̎擾
	ret = m_audio_client->GetService(IID_PPV_ARGS(&m_render_client));
	assert(succeeded(ret));

	// WASAPI���擾
	UINT32 frame = 0;
	ret = m_audio_client->GetBufferSize(&frame);
	assert(succeeded(ret));


	// �[���N���A
	LPBYTE pData;
	ret = m_render_client->GetBuffer(frame, &pData);
	assert(succeeded(ret));

	UINT32 size = frame * m_format.Format.nBlockAlign;
	ZeroMemory(pData, frame * m_format.Format.nBlockAlign);
	ret = m_render_client->ReleaseBuffer(frame, 0);
	assert(succeeded(ret));

	m_current = 0;
	{
#if 0
		btr::BufferMemoryDescriptorEx<int16_t> desc;
		desc.element_num = size / sizeof(int16_t) * 3;
		m_buffer = context->m_staging_memory.allocateMemory(desc);

		// debug�p�̉��𓊓�
		for (int i = 0; i < desc.element_num; i++)
		{
			float a = 128.f / desc.element_num;
			m_buffer.getMappedPtr()[i] = sin(i%128 * a)*20000;
		}
		memcpy(m_buffer.getMappedPtr(), wave.getData(), wave.getLength());
#else
		rWave wave("..\\..\\resource\\010_sound\\Alesis-Fusion-Steel-String-Guitar-C4.wav");

		{
			std::vector<int32_t> buf(((float)m_format.Format.nSamplesPerSec / wave.getFormat()->nSamplesPerSec) * wave.getLength() / 2);

			auto* src_data = (int16_t*)wave.getData();
			float rate = (float)wave.getFormat()->nSamplesPerSec / m_format.Format.nSamplesPerSec;
			int32_t dst_index = 0;
			for (int32_t dst_index = 0; dst_index < buf.size(); dst_index++)
			{
				float src_index = dst_index * rate;
				float lerp_rate = src_index - floor(src_index);
				int i = floor(src_index);
				buf[dst_index] = glm::lerp<float>(src_data[i], src_data[i+1], lerp_rate) * 65000;
			}

			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = buf.size();
			m_buffer = context->m_staging_memory.allocateMemory(desc);

			memcpy(m_buffer.getMappedPtr(), buf.data(), vector_sizeof(buf));
		}

		// @Todo ���L���[�h����m_format(48.0kHz)�ƃf�[�^�̎��g��(44.1kHz)���Ⴄ�̂ŉ���������������B
		// �f�[�^��ϊ������Ƃ��K�v
#endif	
	}

	ret = m_audio_client->Start();
	assert(succeeded(ret));

}


void sSoundSystem::execute_loop(const std::shared_ptr<btr::Context>& context)
{
//	DEBUG("�X���b�h�J�n\n");

	UINT32 audio_buffer_size;
	HRESULT hr = m_audio_client->GetBufferSize(&audio_buffer_size);
	assert(succeeded(hr));

	while (true)
	{

		// �\�[�X�o�b�t�@�̃|�C���^���擾
		auto* src = m_buffer.getMappedPtr();

		// event driven
		if (m_event)
		{
			DWORD retval = WaitForSingleObject(m_event, 2000);
			if (retval != WAIT_OBJECT_0) {
				m_audio_client->Stop();
				break;
			}
			LPBYTE dst;
			hr = m_render_client->GetBuffer(audio_buffer_size, &dst);
			assert(succeeded(hr));


			int buf_size = audio_buffer_size * m_format.Format.nBlockAlign;
			auto copy = std::min<uint32_t>(m_buffer.getInfo().range - m_current, buf_size);
			memcpy(&dst[0], &src[m_current/sizeof(int16_t)], copy);
			auto mod = buf_size - copy;
			if (mod > 0) {
				memcpy(&dst[copy], &src[0], mod);
			}
			m_current = (m_current + buf_size) % m_buffer.getInfo().range;

			// �o�b�t�@�ɏ������񂾂��Ƃ�ʒm
			hr = m_render_client->ReleaseBuffer(audio_buffer_size, 0);
			assert(succeeded(hr));

		}
		// timer driven
		else 
		{

			uint32_t padding = 0;
			auto hr = m_audio_client->GetCurrentPadding(&padding);
			assert(succeeded(hr));

			if (padding == audio_buffer_size)
			{
				// �󂫂Ȃ�
				continue;
			}
			LPBYTE dst;
			hr = m_render_client->GetBuffer(audio_buffer_size-padding, &dst);
			assert(succeeded(hr));

			int buf_size = (audio_buffer_size - padding) * m_format.Format.nBlockAlign;
			auto copy = std::min<uint32_t>(m_buffer.getInfo().range - m_current, buf_size);
			memcpy(&dst[0], &src[m_current/m_buffer.getDataSizeof()], copy);
			auto mod = buf_size - copy;
			if (mod > 0) {
				memcpy(&dst[copy], &src[0], mod);
			}
			m_current = (m_current + buf_size) % m_buffer.getInfo().range;

			// �o�b�t�@�ɏ������񂾂��Ƃ�ʒm
			hr = m_render_client->ReleaseBuffer(audio_buffer_size - padding, 0);
			assert(succeeded(hr));
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

		}

	}

}