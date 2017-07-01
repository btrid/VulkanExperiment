#pragma once
#include <memory>
#include <vector>
#include <btrlib/cModel.h>
struct cModelRenderPrivate;
struct cModelRender
{
	struct PlayMotionDescriptor
	{
		std::shared_ptr<Motion> m_data;
		uint32_t m_play_no;
		uint32_t m_is_loop;
		float m_start_time;

	};
	std::unique_ptr<cModelRenderPrivate> m_private;

public:
	cModelRender();
	~cModelRender();
	void setup(std::shared_ptr<btr::Loader>& loader, std::shared_ptr<cModel::Resource> resource);
	void execute(std::shared_ptr<btr::Executer>& executer);
	void draw(vk::CommandBuffer cmd);
	void play(const PlayMotionDescriptor& desc);

	std::unique_ptr<cModelRenderPrivate>& getPrivate();
};