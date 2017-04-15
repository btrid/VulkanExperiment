#pragma once
#include <btrlib/cModel.h>
#include <002_model/cModelRenderer.h>

struct cModelRenderer;
class ModelRender : public cModel
{
	vk::DescriptorSet m_draw_descriptor_set_per_model;
	std::vector<vk::DescriptorSet> m_draw_descriptor_set_per_mesh;
	std::vector<vk::DescriptorSet> m_compute_descriptor_set;
public:
	void setup(cModelRenderer& renderer);
	void execute(cModelRenderer& renderer, vk::CommandBuffer& cmd);
	void draw(cModelRenderer& renderer, vk::CommandBuffer& cmd);

protected:
private:
};

struct cModelRenderer : public cModelRenderer_t<ModelRender>
{
	std::vector<ModelRender*> m_model;
	void addModel(ModelRender* model)
	{
		m_model.emplace_back(model);
		m_model.back()->setup(*this);
	}
	void execute(vk::CommandBuffer cmd)
	{
		auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
		{
			CameraGPU camera_gpu;
			camera_gpu.setup(*m_camera);
			m_draw_pipeline.m_camera_uniform.update(camera_gpu);
		}
		for (auto& render : m_model)
		{
			render->execute(*this, cmd);
		}

	}
	void draw(vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_draw_pipeline.m_pipeline);
		// draw
		for (auto& render : m_model)
		{
			render->draw(*this, cmd);
		}

	}

};
