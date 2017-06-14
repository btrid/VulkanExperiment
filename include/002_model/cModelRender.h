#pragma once
#include <btrlib/cModelInstancing.h>
#include <btrlib/Light.h>
#include <btrlib/BufferMemory.h>
#include <002_model/cModelPipeline.h>
#include <002_model/cLightPipeline.h>
struct cModelRenderer;
class ModelRender
{
	std::shared_ptr<cModelInstancing::Resource> m_resource;
	std::vector<cModelInstancing*> m_model;

	vk::DescriptorSet m_draw_descriptor_set_per_model;
	std::vector<vk::DescriptorSet> m_draw_descriptor_set_per_mesh;
	std::vector<vk::DescriptorSet> m_compute_descriptor_set;
public:
	void setup(std::shared_ptr<cModelInstancing::Resource> resource) { m_resource = resource; }
	void addModel(cModelInstancing* model) { m_model.push_back(model); }

	void setup(cModelRenderer& renderer);
	void execute(cModelRenderer& renderer, vk::CommandBuffer& cmd);
	void draw(cModelRenderer& renderer, vk::CommandBuffer& cmd);

protected:
private:
};

struct LightSample : public Light
{
	LightParam m_param;
	int life;

	LightSample()
	{
		life = std::rand() % 50 + 30;
		m_param.m_position = glm::vec4(glm::ballRand(3000.f), std::rand() % 50 + 500.f);
		m_param.m_emission = glm::vec4(glm::normalize(glm::abs(glm::ballRand(1.f)) + glm::vec3(0.f, 0.f, 0.01f)), 1.f);

	}
	virtual bool update() override
	{
//		life--;
		return life >= 0;
	}

	virtual LightParam getParam()const override
	{
		return m_param;
	}

};

struct cModelRenderer
{
	cDevice m_device;
	cFowardPlusPipeline m_light_pipeline;
	cModelDrawPipeline m_draw_pipeline;
	cModelComputePipeline m_compute_pipeline;
	std::vector<ModelRender*> m_model;

public:
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_uniform_memory;
	btr::BufferMemory m_staging_memory;
	vk::RenderPass m_render_pass;
	cModelRenderer()
	{
		const cGPU& gpu = sThreadLocal::Order().m_gpu;
		auto device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
		m_device = device;

		m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, 1000*1000*20);
		m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, 65535);
		m_staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached, 65535);

	}
	void setup(vk::RenderPass render_pass)
	{
		m_render_pass = render_pass;
		m_light_pipeline.setup(*this);
		m_draw_pipeline.setup(*this);
		m_compute_pipeline.setup(*this);
	}
	void addModel(ModelRender* model)
	{
		m_model.emplace_back(model);
		m_model.back()->setup(*this);
	}
	void execute(vk::CommandBuffer cmd)
	{
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

		m_light_pipeline.execute(cmd);


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

	cModelDrawPipeline& getDrawPipeline() { return m_draw_pipeline; }
	cModelComputePipeline& getComputePipeline() { return m_compute_pipeline; }
	cFowardPlusPipeline& getLight() {
		return m_light_pipeline;
	}
};
