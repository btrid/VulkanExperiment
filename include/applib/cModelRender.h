#pragma once
#include <memory>
#include <vector>
#include <btrlib/cModel.h>
struct cModelRenderPrivate;

struct PlayMotionDescriptor
{
	std::shared_ptr<cMotion> m_data;
	uint32_t m_play_no;
	uint32_t m_is_loop;
	float m_start_time;

	PlayMotionDescriptor()
		: m_play_no(0)
		, m_is_loop(false)
		, m_start_time(0.f)
	{

	}

};
struct MotionPlayList
{
	struct Work
	{
		std::shared_ptr<cMotion> m_motion;
		float m_time;		//!< Ä¶ˆÊ’u
		int m_index;
		bool m_is_playing;	//!< Ä¶’†H

		Work()
			: m_time(0.f)
			, m_is_playing(false)

		{}
	};
	std::array<Work, 8> m_work;

	void execute()
	{
		float dt = sGlobal::Order().getDeltaTime();
		for (auto& work : m_work)
		{
			if (!work.m_is_playing)
			{
				continue;
			}
			work.m_time += dt * work.m_motion->m_ticks_per_second;
		}
	}

	void play(const PlayMotionDescriptor& desc)
	{
		m_work[desc.m_play_no].m_motion = desc.m_data;
		m_work[desc.m_play_no].m_time = desc.m_start_time;
		m_work[desc.m_play_no].m_index = 0;
		m_work[desc.m_play_no].m_is_playing = true;
	}
};

struct ModelTransform
{
	glm::vec3 m_local_scale;
	glm::quat m_local_rotate;
	glm::vec3 m_local_translate;

	glm::mat4 m_global;

	ModelTransform()
		: m_local_scale(1.f)
		, m_local_rotate(1.f, 0.f, 0.f, 0.f)
		, m_local_translate(0.f)
		, m_global(1.f)
	{}
	glm::mat4 calcLocal()const
	{
		return glm::scale(m_local_scale) * glm::toMat4(m_local_rotate) * glm::translate(m_local_translate);
	}
	glm::mat4 calcGlobal()const
	{
		return m_global;
	}
};
struct cModelRender
{
	std::unique_ptr<cModelRenderPrivate> m_private;

public:
	cModelRender();
	~cModelRender();
	void setup(std::shared_ptr<btr::Loader>& loader, std::shared_ptr<cModel::Resource> resource);
	void execute(std::shared_ptr<btr::Executer>& executer);
	void draw(vk::CommandBuffer cmd);

	ModelTransform& getModelTransform();
	MotionPlayList& getMotionList();
	std::unique_ptr<cModelRenderPrivate>& getPrivate();

};