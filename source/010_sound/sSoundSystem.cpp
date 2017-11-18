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

// std::vector<int8_t> convertData(const WAVEFORMATEX* src, const WAVEFORMATEX* dst, int8_t* src_data, uint32_t src_num)
// {
// 	float rate = (float)src->nSamplesPerSec / dst->nSamplesPerSec;
// 	std::vector<int8_t> buf((1.f/rate) * src_num * (src->wBitsPerSample / 8));
// 
// 	void* dst_data = buf.data();
// 	int32_t dst_byte = dst->wBitsPerSample / 8;
// 	int32_t src_byte = src->wBitsPerSample / 8;
// 
// 	float power = 1.f;
// 	if (dst_byte > src_byte) {
// 		power = 1 << (dst_byte - src_byte);
// 	}
// 	else if (dst_byte < src_byte) {
// 		power = 1.f / (1 << (dst_byte - src_byte));
// 	}
// 
// 	for (int32_t dst_index = 0; dst_index < buf.size(); dst_index++)
// 	{
// 		float src_index = dst_index * rate;
// 		float lerp_rate = src_index - floor(src_index);
// 		int i = floor(src_index);
// 		float data1 = src_data[i];
// 		float data2 = src_data[i + 1];
// 		for (int ii = 1; ii < src_byte; ii++)
// 		{
// 			data1 += src_data[i] * (1 << ii);
// 			data2 += src_data[i + 1] * (1 << ii);
// 		}
// 		int32_t data = glm::lerp<float>(data1, data2, lerp_rate) * power;
// 		memcpy(dst_data, ) = ;
// 		(CHAR*)dst_data += dst_byte;
// 	}
// 
// }

void sSoundSystem::setup(std::shared_ptr<btr::Context>& context)
{
	CoInitialize(NULL);

	HRESULT ret;
	ret = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&m_device_enumerator));
	assert(succeeded(ret));

	// デフォルトのデバイスを選択
	ret = m_device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
	assert(succeeded(ret));

	// オーディオクライアント
	ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
	assert(succeeded(ret));
	// フォーマットの構築
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
	int stream_flag = AUDCLNT_STREAMFLAGS_NOPERSIST;
#else
	AUDCLNT_SHAREMODE share_mode = AUDCLNT_SHAREMODE_SHARED;
	int stream_flag = AUDCLNT_STREAMFLAGS_NOPERSIST; // 共有モードはEventDriven動かないらしい
#endif
	// フォーマットのサポートチェック
	WAVEFORMATEX* closest = nullptr;
	ret = m_audio_client->IsFormatSupported(share_mode, (WAVEFORMATEX*)&m_format, &closest);
	if (FAILED(ret)) {
		// 未サポートのフォーマット
		assert(succeeded(ret));
	}
	if (ret == S_FALSE) {
		// 近いフォーマットに変更
		m_format.Format = *closest;
		m_format.Samples.wValidBitsPerSample = m_format.Format.wBitsPerSample;
	}

	// レイテンシ設定
	REFERENCE_TIME default_device_period = 0;
	REFERENCE_TIME minimum_device_period = 0;
	ret = m_audio_client->GetDevicePeriod(&default_device_period, &minimum_device_period);
	printf("デフォルトデバイスピリオド : %I64d (%fミリ秒)\n", default_device_period, default_device_period / 10000.0);
	printf("最小デバイスピリオド       : %I64d (%fミリ秒)\n", minimum_device_period, minimum_device_period / 10000.0);

	// 初期化
	ret = m_audio_client->Initialize(share_mode,
		stream_flag,
		default_device_period,				// デフォルトデバイスピリオド値をセット
		default_device_period,				// デフォルトデバイスピリオド値をセット
		(WAVEFORMATEX*)&m_format,
		NULL);

	if (FAILED(ret)) {
		if (ret == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
			//	バッファサイズアライメントエラーのため修正する
			UINT32 frame = 0;
			ret = m_audio_client->GetBufferSize(&frame);
			default_device_period =
				(REFERENCE_TIME)(10000.0 *			// (REFERENCE_TIME(100ns) / ms)
					1000 *								// (ms / s) *
					frame /								// frames /
					m_format.Format.nSamplesPerSec +	// (frames / s)
					0.5);								// 四捨五入？

														// オーディオクライアントを再生成
			m_audio_client.Release();
			ret = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audio_client);
			assert(succeeded(ret));

			// 再挑戦
			ret = m_audio_client->Initialize(share_mode,
				stream_flag,
				default_device_period,
				default_device_period,
				(WAVEFORMATEX*)&m_format,
				NULL);
		}
		assert(succeeded(ret));
	}

	// レンダラーの取得
	ret = m_audio_client->GetService(IID_PPV_ARGS(&m_render_client));
	assert(succeeded(ret));

	// WASAPI情報取得
	UINT32 frame = 0;
	ret = m_audio_client->GetBufferSize(&frame);
	assert(succeeded(ret));

	// ゼロクリア
	LPBYTE pData;
	ret = m_render_client->GetBuffer(frame, &pData);
	assert(succeeded(ret));

	UINT32 size = frame * m_format.Format.nBlockAlign;
	ZeroMemory(pData, frame * m_format.Format.nBlockAlign);

	ret = m_render_client->ReleaseBuffer(frame, 0);
	assert(succeeded(ret));

	m_current = 0;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	{

		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = frame * sGlobal::FRAME_MAX; // 3f分くらい
		m_buffer = context->m_staging_memory.allocateMemory(desc);

		std::vector<uint32_t> clear(desc.element_num);
		memcpy(m_buffer.getMappedPtr(), clear.data(), vector_sizeof(clear));
	}
	{

		btr::UpdateBufferDescriptor desc;
		desc.device_memory = context->m_uniform_memory;
		desc.staging_memory = context->m_staging_memory;
		desc.element_num = 1;
		desc.frame_max = sGlobal::FRAME_MAX;
		m_sound_play_info.setup(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<SoundPlayRequestData> desc;
		desc.element_num = SOUND_REQUEST_SIZE;
		m_request_buffer = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<uvec3> desc;
		desc.element_num = 1;
		m_request_buffer_index = context->m_storage_memory.allocateMemory(desc);

		cmd->updateBuffer<uvec3>(m_request_buffer_index.getInfo().buffer, m_request_buffer_index.getInfo().offset, uvec3(0u, 1u, 1u));
	}
	{
		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = SOUND_REQUEST_SIZE;
		m_request_buffer_list = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		for (uint32_t i = 0; i < SOUND_REQUEST_SIZE; i++)
		{
			staging.getMappedPtr()[i] = i;
		}

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_request_buffer_list.getInfo().offset);
		copy.setSize(staging.getInfo().range);
		cmd->copyBuffer(staging.getInfo().buffer, m_request_buffer_list.getInfo().buffer, copy);
	}
	{
		btr::BufferMemoryDescriptorEx<SoundFormat> desc;
		desc.element_num = 1;
		m_sound_format = context->m_uniform_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		staging.getMappedPtr()->nAvgBytesPerSec = m_format.Format.nAvgBytesPerSec;
		staging.getMappedPtr()->nBlockAlign = m_format.Format.nAvgBytesPerSec;
		staging.getMappedPtr()->nChannels = m_format.Format.nAvgBytesPerSec;
		staging.getMappedPtr()->nSamplesPerSec = m_format.Format.nAvgBytesPerSec;
		staging.getMappedPtr()->wBitsPerSample = m_format.Format.wBitsPerSample;
		staging.getMappedPtr()->samples_per_frame = m_format.Format.nSamplesPerSec*m_format.Format.nChannels / 60.f;
		staging.getMappedPtr()->frame_num = sGlobal::FRAME_MAX;
		staging.getMappedPtr()->m_buffer_length = frame * sGlobal::FRAME_MAX;
		staging.getMappedPtr()->m_request_length = frame;
		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_sound_format.getInfo().offset);
		copy.setSize(staging.getBufferInfo().range);

		cmd->copyBuffer(staging.getInfo().buffer, m_sound_format.getBufferInfo().buffer, copy);

	}
	{
		{
			auto stage = vk::ShaderStageFlagBits::eCompute;
			std::vector<vk::DescriptorSetLayoutBinding> binding =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(2),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(10),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(11),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(12),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(SOUND_BANK_SIZE)
				.setBinding(20),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(SOUND_BANK_SIZE)
				.setBinding(21),
			};
			m_descriptor_set_layout = btr::Descriptor::createDescriptorSetLayout(context, binding);
			m_descriptor_pool = btr::Descriptor::createDescriptorPool(context, binding, 1);

			vk::DescriptorSetLayout layouts[] = { m_descriptor_set_layout.get() };
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.setDescriptorPool(m_descriptor_pool.get());
			alloc_info.setDescriptorSetCount(array_length(layouts));
			alloc_info.setPSetLayouts(layouts);
			m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(alloc_info)[0]);
		}

		{
			vk::DescriptorBufferInfo uniforms[] = {
				m_sound_format.getInfo(),
				m_sound_play_info.getBufferInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				m_buffer.getInfo(),
			};
			vk::DescriptorBufferInfo requests[] = {
				m_request_buffer_index.getInfo(),
				m_request_buffer_list.getInfo(),
				m_request_buffer.getInfo(),
			};

			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(2)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(requests))
				.setPBufferInfo(requests)
				.setDstBinding(10)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(write_desc, {});
		}

		// setup shader
		{
			struct
			{
				const char* name;
			}shader_info[] =
			{
				{ "SoundPlay.comp.spv" },
				{ "SoundUpdate.comp.spv" },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PIPELINE_LAYOUT_SOUND_UPDATE] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange constants[] =
				{
					vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute)
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
				pipeline_layout_info.setPPushConstantRanges(constants);
				m_pipeline_layout[PIPELINE_LAYOUT_SOUND_PLAY] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}

		}
		// pipeline
		{
			vk::PipelineShaderStageCreateInfo shader_info[SHADER_NUM];
			for (size_t i = 0; i < SHADER_NUM; i++) {
				shader_info[i].setModule(m_shader_module[i].get());
				shader_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[i].setPName("main");
			}

//			vk::ComputePipelineCreateInfo compute_pipeline_info[] =
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_SOUND_PLAY])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOUND_PLAY].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_SOUND_UPDATE])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOUND_UPDATE].get()),
			};
			auto pipelines = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			std::copy(std::make_move_iterator(pipelines.begin()), std::make_move_iterator(pipelines.end()), m_pipeline.begin());

		}

	}

	ret = m_audio_client->Start();
	assert(succeeded(ret));

}

void sSoundSystem::setSoundbank(const std::shared_ptr<btr::Context>& context, std::vector<std::shared_ptr<SoundBuffer>>&& bank)
{

	std::copy(std::make_move_iterator(bank.begin()), std::make_move_iterator(bank.end()), m_soundbank.begin());

	vk::DescriptorBufferInfo soundinfos[SOUND_BANK_SIZE] = {};
	vk::DescriptorBufferInfo sounddatas[SOUND_BANK_SIZE] = {};
	vk::WriteDescriptorSet write_desc[SOUND_BANK_SIZE*2] = {};
	for (uint32_t i = 0; i < bank.size(); i++)
	{
		soundinfos[i] = m_soundbank[i]->m_info.getInfo();
		sounddatas[i] = m_soundbank[i]->m_buffer.getInfo();
		write_desc[i * 2].setDescriptorType(vk::DescriptorType::eUniformBuffer);
		write_desc[i * 2].setDescriptorCount(1);
		write_desc[i * 2].setPBufferInfo(&soundinfos[i]);
		write_desc[i * 2].setDstBinding(20);
		write_desc[i * 2].setDstArrayElement(i);
		write_desc[i * 2].setDstSet(m_descriptor_set.get());

		write_desc[i * 2 + 1].setDescriptorType(vk::DescriptorType::eStorageBuffer);
		write_desc[i * 2 + 1].setDescriptorCount(1);
		write_desc[i * 2 + 1].setPBufferInfo(&sounddatas[i]);
		write_desc[i * 2 + 1].setDstBinding(21);
		write_desc[i * 2 + 1].setDstArrayElement(i);
		write_desc[i * 2 + 1].setDstSet(m_descriptor_set.get());
	}

	// 空きスペースは適当なもので埋める。todo 空データ準備
	for (uint32_t i = bank.size(); i < SOUND_BANK_SIZE; i++)
	{
		write_desc[i * 2].setDescriptorType(vk::DescriptorType::eUniformBuffer);
		write_desc[i * 2].setDescriptorCount(1);
		write_desc[i * 2].setPBufferInfo(&soundinfos[0]);
		write_desc[i * 2].setDstBinding(20);
		write_desc[i * 2].setDstArrayElement(i);
		write_desc[i * 2].setDstSet(m_descriptor_set.get());

		write_desc[i * 2 + 1].setDescriptorType(vk::DescriptorType::eStorageBuffer);
		write_desc[i * 2 + 1].setDescriptorCount(1);
		write_desc[i * 2 + 1].setPBufferInfo(&sounddatas[0]);
		write_desc[i * 2 + 1].setDstBinding(21);
		write_desc[i * 2 + 1].setDstArrayElement(i);
		write_desc[i * 2 + 1].setDstSet(m_descriptor_set.get());
	}

	context->m_device->updateDescriptorSets(SOUND_BANK_SIZE *2, write_desc, 0, nullptr);

}

std::weak_ptr<SoundPlayRequestData> sSoundSystem::playOneShot(int32_t play_sound_id)
{
	auto data = std::make_shared<SoundPlayRequestData>();
	data->m_play_sound_id = play_sound_id;
	data->m_current = 0;
	data->m_position = vec4(0.f);
	std::weak_ptr<SoundPlayRequestData> weak = data;
	m_sound_request_data_cpu.push_back(std::move(data));
	return weak;
}

vk::CommandBuffer sSoundSystem::execute_loop(const std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
	UINT32 audio_buffer_size;
	auto hr = m_audio_client->GetBufferSize(&audio_buffer_size);
	assert(succeeded(hr));

	uint32_t padding = 0;
	hr = m_audio_client->GetCurrentPadding(&padding);
	assert(succeeded(hr));

	if (padding == audio_buffer_size)
	{
		// やることない
		cmd.end();
		return cmd;
	}

	auto request_size = audio_buffer_size - padding;

	{
		// bufferの0埋め
		auto offset = m_current + (audio_buffer_size*(sGlobal::FRAME_MAX - 1));
		offset %= m_buffer.getInfo().range;
		auto range = glm::min<uint32_t>(request_size, m_buffer.getInfo().range - offset);
		cmd.fillBuffer(m_buffer.getInfo().buffer, m_buffer.getInfo().offset + offset, range, 0u);
		if (padding > range) {
			// 残りは前から詰める
			cmd.fillBuffer(m_buffer.getInfo().buffer, m_buffer.getInfo().offset, padding - range, 0u);
		}

		{
			auto to_transfer = m_sound_play_info.getBufferMemory().makeMemoryBarrierEx();
			to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		}

		SoundPlayInfo info;
		info.m_listener = vec4(0.f);
		info.m_direction = vec4(0.f);
		info.m_sound_deltatime = request_size;
		info.m_write_start = offset;
		m_sound_play_info.subupdate(&info, 1, 0, sGlobal::Order().getGPUIndex());
		auto copy = m_sound_play_info.update(sGlobal::Order().getGPUIndex());
		cmd.copyBuffer(m_sound_play_info.getStagingBufferInfo().buffer, m_sound_play_info.getBufferInfo().buffer, copy);
		{
			auto to_read = m_sound_play_info.getBufferMemory().makeMemoryBarrierEx();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
	}

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_SOUND_PLAY].get());
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOUND_PLAY].get(), 0, m_descriptor_set.get(), {});
	for (uint i = 0; i < SOUND_BANK_SIZE; i++)
	{
		cmd.pushConstants<uint32_t>(m_pipeline_layout[PIPELINE_LAYOUT_SOUND_PLAY].get(), vk::ShaderStageFlagBits::eCompute, 0, i);
		cmd.dispatch(1024, 1, 1);
	}
	do
	{
		// ソースバッファのポインタを取得
		auto* src = m_buffer.getMappedPtr();

		LPBYTE dst;
		hr = m_render_client->GetBuffer(request_size, &dst);
		assert(succeeded(hr));

		int buf_size = request_size * m_format.Format.nBlockAlign;
		auto copy = std::min<uint32_t>(m_buffer.getInfo().range - m_current, buf_size);
		memcpy(&dst[0], &src[m_current/m_buffer.getDataSizeof()], copy);
		auto mod = buf_size - copy;
		if (mod > 0) {
			memcpy(&dst[copy], &src[0], mod);
		}
		m_current = (m_current + buf_size) % m_buffer.getInfo().range;

		// バッファに書き込んだことを通知
		hr = m_render_client->ReleaseBuffer(request_size, 0);
		assert(succeeded(hr));

	} while (false);

	cmd.end();
	return cmd;
}