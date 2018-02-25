#pragma once

#include <vector>
#include <array>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/cModel.h>
#include <applib/cAppModel.h>
#include <applib/DrawHelper.h>
#include <applib/GraphicsResource.h>

struct cModelPipeline
{
//	std::shared_ptr<ModelDrawPipelineComponent> m_pipeline;

//	std::shared_ptr<Model> createRender(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource);

//	void setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<ModelDrawPipelineComponent>& pipeline = nullptr);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

};

