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
	int stream_flag = AUDCLNT_STREAMFLAGS_NOPERSIST;
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

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	{

		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = m_format.Format.nSamplesPerSec*m_format.Format.nChannels / 60.f * 3.f; // 3f�����炢
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
		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = 1;
		m_request_buffer_index = context->m_storage_memory.allocateMemory(desc);

		cmd->fillBuffer(m_request_buffer_index.getInfo().buffer, m_request_buffer_index.getInfo().offset, m_request_buffer_index.getInfo().range, 0u);
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
		staging.getMappedPtr()->frame_num = 3;
		
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
				.setBinding(20),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(21),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(22),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setBinding(100),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(SOUND_BANK_SIZE)
				.setBinding(101),
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
				.setDstBinding(2),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(requests))
				.setPBufferInfo(requests)
				.setDstBinding(20)
				.setDstSet(m_descriptor_set.get()),
			};
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
		write_desc[i * 2].setDstBinding(100);
		write_desc[i * 2].setDstArrayElement(i);
		write_desc[i * 2].setDstSet(m_descriptor_set.get());

		write_desc[i * 2 + 1].setDescriptorType(vk::DescriptorType::eStorageBuffer);
		write_desc[i * 2 + 1].setDescriptorCount(1);
		write_desc[i * 2 + 1].setPBufferInfo(&sounddatas[i]);
		write_desc[i * 2 + 1].setDstBinding(101);
		write_desc[i * 2 + 1].setDstArrayElement(i);
		write_desc[i * 2 + 1].setDstSet(m_descriptor_set.get());
	}

	context->m_device->updateDescriptorSets(bank.size()*2,  write_desc, 0, nullptr);

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
		// ��邱�ƂȂ�
		return cmd;
	}


	do
	{
		// �\�[�X�o�b�t�@�̃|�C���^���擾
		auto* src = m_buffer.getMappedPtr();

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

	} while (false);

	return cmd;
}