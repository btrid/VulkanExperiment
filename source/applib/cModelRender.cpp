#include <applib/cModelRender.h>
#include <applib/cModelRenderPrivate.h>

cModelRender::cModelRender() = default;
cModelRender::~cModelRender() = default;


void cModelRender::setup(std::shared_ptr<btr::Context>& loader, std::shared_ptr<cModel::Resource> resource)
{
	m_private = std::make_unique<cModelRenderPrivate>();
	m_private->setup(loader, resource);
}
void cModelRender::work()
{
	m_private->work();
}

ModelTransform& cModelRender::getModelTransform()
{
	return m_private->m_model_transform;
}

MotionPlayList& cModelRender::getMotionList()
{
	return m_private->m_playlist;
}

std::unique_ptr<cModelRenderPrivate>& cModelRender::getPrivate()
{
	return m_private;
}
