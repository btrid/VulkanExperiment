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

struct Component
{
	virtual ~Component() = default;
};

struct RenderComponent : public Component {};
struct Drawable : public Component {};

struct AnimationModule
{
	virtual btr::BufferMemory getBoneBuffer()const = 0;
	virtual void update() = 0;
	virtual void execute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd) = 0;
};

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
		float m_time;		//!< 再生位置
		int m_index;
		bool m_is_playing;	//!< 再生中？

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
struct DefaultAnimationModule : public AnimationModule
{
	DefaultAnimationModule(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource)
	{
		m_model_resource = resource;
		// bone memory allocate
		{
			btr::UpdateBufferExDescriptor desc;
			desc.alloc_size = (uint32_t)(resource->mBone.size() * sizeof(glm::mat4));
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.frame_max = sGlobal::FRAME_MAX;
			m_bone_buffer_staging.resize(sGlobal::FRAME_MAX);
			for (auto& buffer : m_bone_buffer_staging)
			{
				btr::AllocatedMemory::Descriptor desc;
				desc.size = resource->mBone.size() * sizeof(glm::mat4);
				buffer = context->m_staging_memory.allocateMemory(desc);
			}
			{
				btr::AllocatedMemory::Descriptor desc;
				desc.size = resource->mBone.size() * sizeof(glm::mat4);
				m_bone_buffer = context->m_storage_memory.allocateMemory(desc);
			}
		}

		// recode command
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = sGlobal::FRAME_MAX;
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			m_bone_update_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);
			for (size_t i = 0; i < m_bone_update_cmd.size(); i++)
			{
				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
				vk::CommandBufferInheritanceInfo inheritance_info;
				begin_info.setPInheritanceInfo(&inheritance_info);

				auto& cmd = m_bone_update_cmd[i].get();
				cmd.begin(begin_info);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, m_bone_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite), {});

				vk::BufferCopy copy;
				copy.dstOffset = m_bone_buffer.getBufferInfo().offset;
				copy.size = m_bone_buffer.getBufferInfo().range;
				copy.srcOffset = m_bone_buffer_staging[i].getBufferInfo().offset;
				cmd.copyBuffer(m_bone_buffer_staging[i].getBufferInfo().buffer, m_bone_buffer.getBufferInfo().buffer, copy);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, m_bone_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead), {});
				cmd.end();
			}

		}

	}

	std::shared_ptr<cModel::Resource> m_model_resource;

	ModelTransform m_model_transform;
	MotionPlayList m_playlist;

	btr::BufferMemory m_bone_buffer;
	std::vector<btr::BufferMemory> m_bone_buffer_staging;

	std::vector<vk::UniqueCommandBuffer> m_bone_update_cmd;

	MotionPlayList& getPlayList() { return m_playlist; }
	const MotionPlayList& getPlayList()const { return m_playlist; }
	ModelTransform& getModelTransform() { return m_model_transform; }
	const ModelTransform& getModelTransform()const { return m_model_transform; }

	virtual btr::BufferMemory getBoneBuffer()const override { return m_bone_buffer; }
	virtual void update() override
	{
		m_playlist.execute();
		std::vector<glm::mat4> node_buffer(m_model_resource->m_model_info.mNodeNum);

		updateNodeTransform(0, m_model_transform.calcGlobal()*m_model_transform.calcLocal(), node_buffer);

		// シェーダに送るデータを更新
		auto* ptr = m_bone_buffer_staging[sGlobal::Order().getCPUFrame()].getMappedPtr<glm::mat4>();
		for (size_t i = 0; i < m_model_resource->mNodeRoot.mNodeList.size(); i++)
		{
			auto* node = m_model_resource->mNodeRoot.getNodeByIndex(i);
			int32_t bone_index = node->m_bone_index;
			if (bone_index >= 0)
			{
				auto m = node_buffer[i] * m_model_resource->mBone[bone_index].mOffset;
				ptr[bone_index] = m;
			}
		}
	}
	virtual void execute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd) override
	{
		cmd.executeCommands(m_bone_update_cmd[context->getGPUFrame()].get());
	}

private:
	void updateNodeTransform(uint32_t node_index, const glm::mat4& parentMat, std::vector<glm::mat4>& node_buffer)
	{
		auto* node = m_model_resource->mNodeRoot.getNodeByIndex(node_index);
		auto& work = m_playlist.m_work[0];
		auto target_time = m_playlist.m_work[0].m_time;
		auto& anim = m_playlist.m_work[0].m_motion;
		auto transformMatrix = parentMat;
		if (anim)
		{
			cMotion::NodeMotion* nodeAnim = nullptr;
			for (auto& a : anim->m_data)
			{
				if (a.m_nodename == node->mName)
				{
					nodeAnim = &a;
					break;
				}
			}

			if (nodeAnim)
			{
				//@Todo Index0から見てるので遅い
				size_t pos_index = 0;
				size_t rot_index = 0;
				size_t scale_index = 0;
				auto translate_value = nodeAnim->m_translate[nodeAnim->m_translate.size() - 1].m_data;
				auto rotate_value = nodeAnim->m_rotate[nodeAnim->m_rotate.size() - 1].m_data;
				auto scale_value = nodeAnim->m_scale[nodeAnim->m_scale.size() - 1].m_data;
				for (; pos_index < nodeAnim->m_translate.size() - 1; pos_index++) {
					if (nodeAnim->m_translate[pos_index + 1].m_time >= target_time) {
						float rate = (target_time - nodeAnim->m_translate[pos_index].m_time) / (nodeAnim->m_translate[pos_index + 1].m_time - nodeAnim->m_translate[pos_index].m_time);
						translate_value = glm::lerp(nodeAnim->m_translate[pos_index].m_data, nodeAnim->m_translate[pos_index + 1].m_data, rate);
						break;
					}
				}
				for (; rot_index < nodeAnim->m_rotate.size() - 1; rot_index++) {
					if (nodeAnim->m_rotate[rot_index + 1].m_time >= target_time) {
						float rate = (target_time - nodeAnim->m_rotate[rot_index].m_time) / (nodeAnim->m_rotate[rot_index + 1].m_time - nodeAnim->m_rotate[rot_index].m_time);
						rotate_value = glm::lerp(nodeAnim->m_rotate[rot_index].m_data, nodeAnim->m_rotate[rot_index + 1].m_data, rate);
						break;
					}
				}
				for (; scale_index < nodeAnim->m_scale.size() - 1; scale_index++) {
					if (nodeAnim->m_scale[scale_index + 1].m_time >= target_time) {
						float rate = (target_time - nodeAnim->m_scale[scale_index].m_time) / (nodeAnim->m_scale[scale_index + 1].m_time - nodeAnim->m_scale[scale_index].m_time);
						scale_value = glm::lerp(nodeAnim->m_scale[scale_index].m_data, nodeAnim->m_scale[scale_index + 1].m_data, rate);
						break;
					}
				}
				transformMatrix = parentMat * glm::scale(scale_value) * glm::translate(translate_value)* glm::toMat4(rotate_value);
				node_buffer[node_index] = transformMatrix;
			}
		}
		for (auto& n : node->mChildren) {
			updateNodeTransform(n, node_buffer[node_index], node_buffer);
		}
	}

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


	void draw(std::shared_ptr<btr::Context>& executer, vk::CommandBuffer& cmd)
	{
		cmd.executeCommands(m_draw_cmd[executer->getGPUFrame()].get());
	}

};

struct Model : public Drawable
{
	std::shared_ptr<DefaultMaterialModule> m_material;
	std::shared_ptr<DefaultAnimationModule> m_animation;
	
	std::shared_ptr<ModelRender> m_render;
};


struct cModelPipeline
{
	enum {
		DESCRIPTOR_TEXTURE_NUM = 16,
	};
	enum {
		SHADER_RENDER_VERT,
		SHADER_RENDER_FRAG,
		SHADER_NUM,
	};
	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MODEL,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};

	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::UniquePipeline m_render_pipeline;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	vk::UniqueDescriptorPool m_model_descriptor_pool;

	std::vector<std::shared_ptr<Model>> m_model;
	void setup(std::shared_ptr<btr::Context>& context);

	std::shared_ptr<Model> createRender(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource);

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer);

};
