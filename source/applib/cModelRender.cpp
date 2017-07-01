#include <applib/cModelRender.h>
#include <applib/cModelRenderPrivate.h>

cModelRender::cModelRender() = default;
cModelRender::~cModelRender() = default;


void cModelRender::setup(std::shared_ptr<btr::Loader>& loader, std::shared_ptr<cModel::Resource> resource)
{
	m_private = std::make_unique<cModelRenderPrivate>();
	m_private->setup(loader, resource);
}
void cModelRender::execute(std::shared_ptr<btr::Executer>& executer)
{
	m_private->execute(executer);
}
void cModelRender::draw(vk::CommandBuffer cmd)
{
	m_private->draw(cmd);
}

void cModelRender::play(const PlayMotionDescriptor& desc)
{
	m_private->m_playlist.m_work[desc.m_play_no].m_motion = desc.m_data;
	m_private->m_playlist.m_work[desc.m_play_no].m_time = desc.m_start_time;
	m_private->m_playlist.m_work[desc.m_play_no].m_index = 0;
	m_private->m_playlist.m_work[desc.m_play_no].m_is_playing = true;
}

std::unique_ptr<cModelRenderPrivate>& cModelRender::getPrivate()
{
	return m_private;
}
