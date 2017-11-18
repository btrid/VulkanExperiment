#pragma once

#include <atlbase.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

#include <memory>

#include <btrlib/Context.h>

#include <010_sound/rWave.h>

struct SoundInfo
{
	uint32_t m_length;
	uint32_t m_samples_per_sec;
};

struct SoundBuffer
{
	SoundBuffer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<rWave>& wave);

	btr::BufferMemoryEx<SoundInfo> m_info;
	btr::BufferMemoryEx<int32_t> m_buffer;
};