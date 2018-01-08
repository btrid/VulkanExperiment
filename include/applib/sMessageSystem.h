#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

struct iMessage{
	uint32_t id;
	virtual ~iMessage() = default;
};
struct sMessageSystem : Singleton<sMessageSystem>
{
	friend Singleton<sMessageSystem>;

	enum class Message
	{
		SE_CALL,
	};
	struct SERequest 
	{
		int32_t id;
		glm::vec3 pos;
	};
	btr::BufferMemoryEx<uint32_t> m_se_request_counter;
	btr::BufferMemoryEx<SERequest> m_se_request_buffer;

	void setup(const std::shared_ptr<btr::Context>& context)
	{
		{
			{
				btr::BufferMemoryDescriptorEx<uint32_t> desc;
				desc.element_num = 1;
				m_se_request_counter = context->m_staging_memory.allocateMemory(desc);
			}
			{
				btr::BufferMemoryDescriptorEx<SERequest> desc;
				desc.element_num = 128;
				m_se_request_buffer = context->m_staging_memory.allocateMemory(desc);
			}
		}
	}
};