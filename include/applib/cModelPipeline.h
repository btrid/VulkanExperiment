#pragma once

#include <vector>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/cModel.h>
#include <applib/DrawHelper.h>

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

struct AnimationModule
{
	ModelTransform m_model_transform;
	MotionPlayList m_playlist;

	ModelTransform& getTransform() { return m_model_transform; }
	const ModelTransform& getTransform()const { return m_model_transform; }
	MotionPlayList& getPlayList() { return m_playlist; }
	const MotionPlayList& getPlayList()const { return m_playlist; }

	virtual btr::BufferMemory getBoneBuffer()const = 0;
	virtual void update() = 0;
	virtual void execute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd) = 0;
};

struct DefaultMaterialModule : public MaterialModule
{
	struct MaterialBuffer {
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		glm::vec4		mEmissive;
		uint32_t		u_albedo_texture;
		uint32_t		u_ambient_texture;
		uint32_t		u_specular_texture;
		uint32_t		u_emissive_texture;
		float			mShininess;
		float			__p;
		float			__p1;
		float			__p2;
	};

	btr::BufferMemory m_material_index;
	btr::BufferMemory m_material;

	DefaultMaterialModule(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		// material index
		{
			btr::AllocatedMemory::Descriptor staging_desc;
			staging_desc.size = resource->m_mesh.size() * sizeof(uint32_t);
			staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(staging_desc);

			std::vector<uint32_t> material_index(resource->m_mesh.size());
			for (size_t i = 0; i < material_index.size(); i++)
			{
				staging.getMappedPtr<uint32_t>()[i] = resource->m_mesh[i].m_material_index;
			}

			m_material_index = context->m_storage_memory.allocateMemory(material_index.size() * sizeof(uint32_t));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(m_material_index.getBufferInfo().offset);
			cmd->copyBuffer(staging.getBufferInfo().buffer, m_material_index.getBufferInfo().buffer, copy_info);

		}

		// material
		{
			btr::AllocatedMemory::Descriptor staging_desc;
			staging_desc.size = resource->m_material.size() * sizeof(MaterialBuffer);
			staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_material = context->m_staging_memory.allocateMemory(staging_desc);
			auto* mb = static_cast<MaterialBuffer*>(staging_material.getMappedPtr());
			for (size_t i = 0; i < resource->m_material.size(); i++)
			{
				mb[i].mAmbient = resource->m_material[i].mAmbient;
				mb[i].mDiffuse = resource->m_material[i].mDiffuse;
				mb[i].mEmissive = resource->m_material[i].mEmissive;
				mb[i].mSpecular = resource->m_material[i].mSpecular;
				mb[i].mShininess = resource->m_material[i].mShininess;
			}

			m_material = context->m_storage_memory.allocateMemory(resource->m_material.size() * sizeof(MaterialBuffer));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_material.getBufferInfo().range);
			copy_info.setSrcOffset(staging_material.getBufferInfo().offset);
			copy_info.setDstOffset(m_material.getBufferInfo().offset);
			cmd->copyBuffer(staging_material.getBufferInfo().buffer, m_material.getBufferInfo().buffer, copy_info);
		}

	}

	virtual btr::BufferMemory getMaterialIndexBuffer()const override { return m_material_index; }
	virtual btr::BufferMemory getMaterialBuffer()const override { return m_material; }
};


struct ModelRender : public RenderComponent
{
	std::vector<vk::UniqueCommandBuffer> m_draw_cmd;
	vk::UniqueDescriptorSet m_descriptor_set_model;

	void draw(std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd)
	{
		cmd.executeCommands(m_draw_cmd[context->getGPUFrame()].get());
	}

};

struct Model : public Drawable
{
	std::shared_ptr<cModel::Resource> m_model_resource;
	std::shared_ptr<MaterialModule> m_material;
	std::shared_ptr<AnimationModule> m_animation;
	
	std::shared_ptr<ModelRender> m_render;
};
struct ModelPipelineComponent : public PipelineComponent
{
	virtual std::shared_ptr<ModelRender> createRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<Model>& model) = 0;
	virtual const std::shared_ptr<RenderPassModule>& getRenderPassModule()const = 0;
	virtual vk::Pipeline getPipeline()const = 0;
};

struct DescriptorModule
{
protected:
	vk::UniqueDescriptorSetLayout createDescriptorSetLayout(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
		descriptor_layout_info.setBindingCount((uint32_t)binding.size());
		descriptor_layout_info.setPBindings(binding.data());
		return context->m_device->createDescriptorSetLayoutUnique(descriptor_layout_info);
	}
	vk::UniqueDescriptorPool createDescriptorPool(const std::shared_ptr<btr::Context>& context, const std::vector<vk::DescriptorSetLayoutBinding>& binding, uint32_t set_size)
	{
		auto& device = context->m_device;
		std::vector<vk::DescriptorPoolSize> pool_size(VK_DESCRIPTOR_TYPE_RANGE_SIZE);
		for (auto& b : binding)
		{
			pool_size[(uint32_t)b.descriptorType].setType(b.descriptorType);
			pool_size[(uint32_t)b.descriptorType].descriptorCount += b.descriptorCount*set_size;
		}
		pool_size.erase(std::remove_if(pool_size.begin(), pool_size.end(), [](auto& p) {return p.descriptorCount == 0; }), pool_size.end());
		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setPoolSizeCount((uint32_t)pool_size.size());
		pool_info.setPPoolSizes(pool_size.data());
		pool_info.setMaxSets(set_size);
		pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
		return device->createDescriptorPoolUnique(pool_info);
	}
protected:
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorPool m_descriptor_pool;
public:
	vk::DescriptorSetLayout getLayout()const { return m_descriptor_set_layout.get(); }
	vk::DescriptorPool getPool()const { return m_descriptor_pool.get(); }

};
struct ModelDescriptorModule : public DescriptorModule
{
	virtual vk::UniqueDescriptorSet allocateDescriptorSet(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<Model>& model) = 0;
};

struct DefaultModelDescriptorModule : public ModelDescriptorModule
{
	DefaultModelDescriptorModule(const std::shared_ptr<btr::Context>& context)
	{
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(DESCRIPTOR_TEXTURE_NUM)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setBinding(5),
		};
		m_descriptor_set_layout = createDescriptorSetLayout(context, binding);
		m_descriptor_pool = createDescriptorPool(context, binding, 30);
	}

	vk::UniqueDescriptorSet allocateDescriptorSet(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<Model>& model) override
	{
		auto& device = context->m_device;
		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(getPool());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		auto descriptor_set = std::move(device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);

		std::vector<vk::DescriptorBufferInfo> storages = {
			model->m_animation->getBoneBuffer().getBufferInfo(),
			model->m_material->getMaterialIndexBuffer().getBufferInfo(),
			model->m_material->getMaterialBuffer().getBufferInfo(),
		};

		std::vector<vk::DescriptorImageInfo> color_images(DESCRIPTOR_TEXTURE_NUM, vk::DescriptorImageInfo(DrawHelper::Order().getWhiteTexture().m_sampler.get(), DrawHelper::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		for (size_t i = 0; i < model->m_model_resource->m_mesh.size(); i++)
		{
			auto& material = model->m_model_resource->m_material[model->m_model_resource->m_mesh[i].m_material_index];
			color_images[i] = vk::DescriptorImageInfo(material.mDiffuseTex.getSampler(), material.mDiffuseTex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		}
		std::vector<vk::WriteDescriptorSet> drawWriteDescriptorSets =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount((uint32_t)storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(2)
			.setDstSet(descriptor_set.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount((uint32_t)color_images.size())
			.setPImageInfo(color_images.data())
			.setDstBinding(5)
			.setDstSet(descriptor_set.get()),
		};
		device->updateDescriptorSets(drawWriteDescriptorSets, {});
		return descriptor_set;
	}

private:
	enum {
		DESCRIPTOR_TEXTURE_NUM = 16,
	};
};

struct cModelPipeline
{
	std::shared_ptr<ModelPipelineComponent> m_pipeline;

	std::vector<std::shared_ptr<Model>> m_model;
	std::shared_ptr<Model> createRender(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource);

	void setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<ModelPipelineComponent>& pipeline = nullptr);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

};

