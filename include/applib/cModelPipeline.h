#pragma once

#include <vector>
#include <list>
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

struct MaterialModule
{
	virtual btr::BufferMemory getMaterialIndexBuffer()const = 0;
	virtual btr::BufferMemory getMaterialBuffer()const = 0;
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

struct cModelPipeline
{
	std::shared_ptr<ModelPipelineComponent> m_pipeline;

	std::vector<std::shared_ptr<Model>> m_model;
	std::shared_ptr<Model> createRender(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource);

	void setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<ModelPipelineComponent>& pipeline = nullptr);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

};

