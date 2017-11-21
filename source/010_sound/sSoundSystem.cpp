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
	ZeroMemory(pData, size);

	ret = m_render_client->ReleaseBuffer(frame, 0);
	assert(succeeded(ret));

	m_current = 0;

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	{

		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = size/4 * SOUND_BUFFER_FRAME;
		m_buffer = context->m_staging_memory.allocateMemory(desc);

		std::vector<uint32_t> clear(desc.element_num);
		memcpy(m_buffer.getMappedPtr(), clear.data(), vector_sizeof(clear));
	}
	{

		btr::UpdateBufferDescriptor desc;
		desc.device_memory = context->m_storage_memory;
		desc.staging_memory = context->m_staging_memory;
		desc.element_num = 1;
		desc.frame_max = sGlobal::FRAME_MAX;
		m_sound_play_info.setup(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<SoundPlayRequestData> desc;
		desc.element_num = SOUND_REQUEST_SIZE;
		m_playing_buffer = context->m_storage_memory.allocateMemory(desc);
		cmd->fillBuffer(m_playing_buffer.getInfo().buffer, m_playing_buffer.getInfo().offset, m_playing_buffer.getInfo().range, 0u);
		{
			auto to_read = m_playing_buffer.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
		m_request_buffer = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<uvec3> desc;
		desc.element_num = 1;
		m_sound_counter = context->m_storage_memory.allocateMemory(desc);

		cmd->updateBuffer<uvec3>(m_sound_counter.getInfo().buffer, m_sound_counter.getInfo().offset, uvec3(0u, 0u, 0u));
		{
			auto to_read = m_sound_counter.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

	}
	{
		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = SOUND_REQUEST_SIZE;
		m_active_buffer = context->m_storage_memory.allocateMemory(desc);
		m_free_buffer = context->m_storage_memory.allocateMemory(desc);
	}
	{
		btr::BufferMemoryDescriptorEx<SoundFormat> desc;
		desc.element_num = 1;
		m_sound_format = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		staging.getMappedPtr()->nAvgBytesPerSec = m_format.Format.nAvgBytesPerSec;
		staging.getMappedPtr()->nBlockAlign = m_format.Format.nBlockAlign;
		staging.getMappedPtr()->nChannels = m_format.Format.nChannels;
		staging.getMappedPtr()->nSamplesPerSec = m_format.Format.nSamplesPerSec;
		staging.getMappedPtr()->wBitsPerSample = m_format.Format.wBitsPerSample;
		staging.getMappedPtr()->samples_per_frame = m_format.Format.nSamplesPerSec*m_format.Format.nChannels / 60.f;
		staging.getMappedPtr()->frame_num = SOUND_BUFFER_FRAME;
		staging.getMappedPtr()->m_buffer_length = size / 4 * SOUND_BUFFER_FRAME;
		staging.getMappedPtr()->m_request_length = size / 4;
		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_sound_format.getInfo().offset);
		copy.setSize(staging.getInfo().range);

		cmd->copyBuffer(staging.getInfo().buffer, m_sound_format.getInfo().buffer, copy);
		{
			auto to_read = m_sound_format.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

	}
	{
		{
			auto stage = vk::ShaderStageFlagBits::eCompute;
			std::vector<vk::DescriptorSetLayoutBinding> binding =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
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
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(13),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(14),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
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
				m_sound_counter.getInfo(),
				m_active_buffer.getInfo(),
				m_free_buffer.getInfo(),
				m_playing_buffer.getInfo(),
				m_request_buffer.getInfo(),
			};

			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
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
				{ "SoundRequest.comp.spv" },
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
				m_pipeline_layout[PIPELINE_LAYOUT_SOUND_REQUEST] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
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

			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_SOUND_PLAY])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOUND_PLAY].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_SOUND_UPDATE])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOUND_UPDATE].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[SHADER_SOUND_REQUEST])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_SOUND_REQUEST].get()),
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
		write_desc[i * 2].setDescriptorType(vk::DescriptorType::eStorageBuffer);
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
		write_desc[i * 2].setDescriptorType(vk::DescriptorType::eStorageBuffer);
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

	static int is_init;
	if (is_init == 0)
	{
		is_init = 1;
		// sound初期化
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_SOUND_UPDATE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOUND_UPDATE].get(), 0, m_descriptor_set.get(), {});
		cmd.dispatch(1, 1, 1);

		// debug
		{
			SoundPlayRequestData request;
			request.m_play_sound_id = 0;
			request.m_position = vec4(0.f);
			cmd.updateBuffer<SoundPlayRequestData>(m_request_buffer.getInfo().buffer, m_request_buffer.getInfo().offset, request);
			cmd.updateBuffer<uint32_t>(m_sound_counter.getInfo().buffer, m_sound_counter.getInfo().offset + 8, 1);
			{
				auto to_read = m_request_buffer.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
			}
			{
				auto to_read = m_sound_counter.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
			}

		}

	}

	{
		// 再生リクエストの処理
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_SOUND_REQUEST].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOUND_REQUEST].get(), 0, m_descriptor_set.get(), {});
		cmd.dispatch(1, 1, 1);
		{
			auto to_read = m_sound_counter.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
	}

	UINT32 audio_buffer_size;
	auto hr = m_audio_client->GetBufferSize(&audio_buffer_size);
	assert(succeeded(hr));
	UINT32 audio_buffer_byte = audio_buffer_size * m_format.Format.nBlockAlign;

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
	auto request_byte = request_size* m_format.Format.nBlockAlign;
	auto padding_byte = padding* m_format.Format.nBlockAlign;


	{
		// サウンドにデータを渡す
		auto* src = m_buffer.getMappedPtr();

		LPBYTE dst;
		hr = m_render_client->GetBuffer(request_size, &dst);
		assert(succeeded(hr));

		auto copy = std::min<uint32_t>(m_buffer.getInfo().range - m_current, request_byte);
		{
			auto to_transfer = m_buffer.makeMemoryBarrier();
			to_transfer.offset += m_current;
			to_transfer.size = copy;
			to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			to_transfer.setDstAccessMask(vk::AccessFlagBits::eHostRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, {}, to_transfer, {});
		}
		memcpy(&dst[0], &src[m_current / m_buffer.getDataSizeof()], copy);
		auto mod = request_byte - copy;
		if (mod > 0) 
		{
			{
				auto to_transfer = m_buffer.makeMemoryBarrier();
				to_transfer.offset += 0;
				to_transfer.size = mod;
				to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_transfer.setDstAccessMask(vk::AccessFlagBits::eHostRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eHost, {}, {}, to_transfer, {});
			}
			memcpy(&dst[copy], &src[0], mod);
		}

		// バッファに書き込んだことを通知
		hr = m_render_client->ReleaseBuffer(request_size, 0);
		assert(succeeded(hr));
	}

	{
		// bufferの0埋め
		auto offset_byte = m_current;
		offset_byte %= m_buffer.getInfo().range;
		auto copy_byte = glm::min<int32_t>(request_byte, m_buffer.getInfo().range - offset_byte);
		{
			auto to_transfer = m_buffer.makeMemoryBarrier();
			to_transfer.offset += offset_byte;
			to_transfer.size = copy_byte;
			to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});
		}
		cmd.fillBuffer(m_buffer.getInfo().buffer, m_buffer.getInfo().offset + offset_byte, copy_byte, 0u);
		{
			auto to_write = m_buffer.makeMemoryBarrier();
			to_write.offset += offset_byte;
			to_write.size = copy_byte;
			to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_write, {});
		}
		if (request_byte > copy_byte) 
		{
			// 残りは前から詰める
			{
				auto to_transfer = m_buffer.makeMemoryBarrier();
				to_transfer.offset += 0;
				to_transfer.size = (request_byte - copy_byte);
				to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});
			}
			cmd.fillBuffer(m_buffer.getInfo().buffer, m_buffer.getInfo().offset, (request_byte - copy_byte), 0u);
			{
				auto to_write = m_buffer.makeMemoryBarrier();
				to_write.offset += 0;
				to_write.size = (request_byte - copy_byte);
				to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_write, {});
			}
		}

		{
			SoundPlayInfo info;
			info.m_listener = vec4(0.f);
			info.m_direction = vec4(0.f);
			info.m_sound_deltatime = request_byte / 4;
			info.m_write_start = offset_byte / 4;
			m_sound_play_info.subupdate(&info, 1, 0, sGlobal::Order().getGPUIndex());
			auto copy = m_sound_play_info.update(sGlobal::Order().getGPUIndex());
			{
				auto to_transfer = m_sound_play_info.getBufferMemory().makeMemoryBarrierEx();
				to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});
			}
			cmd.copyBuffer(m_sound_play_info.getStagingBufferInfo().buffer, m_sound_play_info.getBufferInfo().buffer, copy);
			{
				auto to_read = m_sound_play_info.getBufferMemory().makeMemoryBarrierEx();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
			}
		}

		{
			auto to_read = m_sound_counter.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead );
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

		// bufferをデータで埋める
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_SOUND_PLAY].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOUND_PLAY].get(), 0, m_descriptor_set.get(), {});
		for (uint i = 0; i < 1; i++)
		{
			cmd.pushConstants<uint32_t>(m_pipeline_layout[PIPELINE_LAYOUT_SOUND_PLAY].get(), vk::ShaderStageFlagBits::eCompute, 0, i);
			cmd.dispatch(1, 1, 1);
		}
	}

	{
		{
			std::array<vk::BufferMemoryBarrier, 2> to_read;
			to_read[0] = m_playing_buffer.makeMemoryBarrier();
			to_read[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_read[0].setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read[1] = m_sound_counter.makeMemoryBarrier();
			to_read[1].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_read[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}

		// sound更新
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_SOUND_UPDATE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_SOUND_UPDATE].get(), 0, m_descriptor_set.get(), {});
		cmd.dispatch(1, 1, 1);

		{
			std::array<vk::BufferMemoryBarrier, 2> to_read;
			to_read[0] = m_playing_buffer.makeMemoryBarrier();
			to_read[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read[0].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			to_read[1] = m_sound_counter.makeMemoryBarrier();
			to_read[1].setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_read[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
	}

	m_current = (m_current + request_byte) % m_buffer.getInfo().range;
	cmd.end();
	return cmd;
}