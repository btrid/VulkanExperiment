#pragma once
#include <btrlib/cModel.h>
#include <btrlib/Light.h>
#include <btrlib/BufferMemory.h>
#include <002_model/cModelRenderer.h>

struct cModelRenderer;
class ModelRender
{
	std::shared_ptr<cModel::Resource> m_resource;
	std::vector<cModel*> m_model;

	vk::DescriptorSet m_draw_descriptor_set_per_model;
	std::vector<vk::DescriptorSet> m_draw_descriptor_set_per_mesh;
	std::vector<vk::DescriptorSet> m_compute_descriptor_set;
public:
	void setup(std::shared_ptr<cModel::Resource> resource) { m_resource = resource; }
	void addModel(cModel* model) { m_model.push_back(model); }

	void setup(cModelRenderer& renderer);
	void execute(cModelRenderer& renderer, vk::CommandBuffer& cmd);
	void draw(cModelRenderer& renderer, vk::CommandBuffer& cmd);

protected:
private:
};

struct LightSample : public btr::Light
{
	btr::LightParam m_param;
	int life;

	LightSample()
	{
		m_param.m_position = glm::vec4(glm::ballRand(3000.f), 1.f);
		m_param.m_emission = glm::vec4(glm::normalize(glm::abs(glm::ballRand(1.f)) + glm::vec3(0.f, 0.f, 0.01f)), 1.f);

		life = std::rand() % 50 + 30;
	}
	virtual bool update() override
	{
		life--;
		return life >= 0;
	}

	virtual btr::LightParam getParam()const override
	{
		return m_param;
	}

};

struct cModelRenderer : public cModelRenderer_t
{
	btr::Light::FowardPlusPipeline m_light_manager;
	std::vector<ModelRender*> m_model;

	cModelRenderer()
	{
		const cGPU& gpu = sThreadLocal::Order().m_gpu;
		auto device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];

		m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, 1000*1000*20);
		m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, 65535);
		m_staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached, 65535);
		m_light_manager.setup(device, m_storage_memory, 1024);

	}
	void addModel(ModelRender* model)
	{
		m_model.emplace_back(model);
		m_model.back()->setup(*this);
	}
	void execute(vk::CommandBuffer cmd)
	{
		m_light_manager.execute(cmd);

		{
			auto* camera = cCamera::sCamera::Order().getCameraList()[0];
			CameraGPU cameraGPU;
			cameraGPU.setup(*camera);
			m_draw_pipeline.m_camera.subupdate(cameraGPU);
			m_draw_pipeline.m_camera.update(cmd);
// 
			Frustom frustom;
			frustom.setup(*camera);
			m_compute_pipeline.m_camera_frustom.subupdate(frustom.getPlane());
			m_compute_pipeline.m_camera_frustom.update(cmd);
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

	btr::Light::FowardPlusPipeline& getLight() {
		return m_light_manager;
	}
};
