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

	// デフォルトのデバイスを選択
	ret = m_device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
	assert(SUCCEEDED(ret));

	// オーディオクライアント
	ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
	assert(SUCCEEDED(ret));

	// 	// フォーマットの構築
	WAVEFORMATEXTENSIBLE wf;
	ZeroMemory(&wf, sizeof(wf));
	wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
	wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wf.Format.nChannels = 2;
	wf.Format.nSamplesPerSec = 44100;
	wf.Format.wBitsPerSample = 16;
	wf.Format.nBlockAlign = wf.Format.nChannels * wf.Format.wBitsPerSample / 8;
	wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
	wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
	wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	wf.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

//	AUDCLNT_SHAREMODE mode = AUDCLNT_SHAREMODE_EXCLUSIVE;
	AUDCLNT_SHAREMODE mode = AUDCLNT_SHAREMODE_SHARED;
	// フォーマットのサポートチェック
	WAVEFORMATEX* closest = nullptr;
	ret = m_audio_client->IsFormatSupported(mode, (WAVEFORMATEX*)&wf, &closest);
	if (FAILED(ret)) {
		// 未サポートのフォーマット
		assert(SUCCEEDED(ret));
	}
	if (ret == S_FALSE) {
		// 近いフォーマットに変更
		wf.Format = *closest;
		wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
	}

	// レイテンシ設定
	REFERENCE_TIME default_device_period = 0;
	REFERENCE_TIME minimum_device_period = 0;
	ret = m_audio_client->GetDevicePeriod(&default_device_period, &minimum_device_period);
	printf("デフォルトデバイスピリオド : %I64d (%fミリ秒)\n", default_device_period, default_device_period / 10000.0);
	printf("最小デバイスピリオド       : %I64d (%fミリ秒)\n", minimum_device_period, minimum_device_period / 10000.0);

	// 初期化
	UINT32 frame = 0;
	ret = m_audio_client->Initialize(mode,
		AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		default_device_period,				// デフォルトデバイスピリオド値をセット
		default_device_period,				// デフォルトデバイスピリオド値をセット
		(WAVEFORMATEX*)&wf.Format,
		NULL);
	if (FAILED(ret)) {
		if (ret == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
//			DEBUG("バッファサイズアライメントエラーのため修正する\n");

			// 修正後のフレーム数を取得
			ret = m_audio_client->GetBufferSize(&frame);
//			DEBUG("修正後のフレーム数         : %d\n", frame);
			default_device_period = (REFERENCE_TIME)(10000.0 *						// (REFERENCE_TIME(100ns) / ms) *
				1000 *						// (ms / s) *
				frame /						// frames /
				wf.Format.nSamplesPerSec +	// (frames / s)
				0.5);							// 四捨五入？
//			DEBUG("修正後のレイテンシ         : %I64d (%fミリ秒)\n", default_device_period, default_device_period / 10000.0);

			// 一度破棄してオーディオクライアントを再生成
//			SAFE_RELEASE(pAudioClient);
			ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
			assert(SUCCEEDED(ret));

			// 再挑戦
			ret = m_audio_client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
				AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
				default_device_period,
				default_device_period,
				(WAVEFORMATEX*)&wf,
				NULL);
		}
		if (FAILED(ret))
		{
			printf(to_string(ret).c_str());
			assert(false);
		}
	}

	// イベント生成
	auto hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	// イベントのセット
	ret = m_audio_client->SetEventHandle(hEvent);
	assert(SUCCEEDED(ret));

	// レンダラーの取得
	ret = m_audio_client->GetService(IID_PPV_ARGS(&m_render_client));
	assert(SUCCEEDED(ret));

	// WASAPI情報取得
	ret = m_audio_client->GetBufferSize(&frame);
	assert(SUCCEEDED(ret));

	UINT32 size = frame * wf.Format.nBlockAlign;

	// ゼロクリアをしてイベントをリセット
	LPBYTE pData;
	ret = m_render_client->GetBuffer(frame, &pData);
	assert(SUCCEEDED(ret));

	ZeroMemory(pData, size);
	m_render_client->ReleaseBuffer(frame, 0);

}
