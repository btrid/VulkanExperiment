#pragma once

#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/System.h>
#include <013_2DGI/GI2D/GI2DContext.h>
#include <013_2DGI/GI2D/GI2DPhysics.h>

struct cMovable
{
	vec2 pos;
	vec2 dir;

	float scale;
	uint rb_address;
	uint _p1;
	uint _p2;
};
struct cState
{
	uint64_t alive;
};

struct DS_GameObject 
{
	uint index;
};
struct GameContext
{
	enum Layout
	{
		DSL_GameObject,
		Layout_Num,
	};
	std::array<vk::UniqueDescriptorSetLayout, Layout_Num> m_descriptor_set_layout;
	vk::DescriptorSetLayout getDescriptorSetLayout(Layout layout)const { return m_descriptor_set_layout[layout].get(); }

	GameContext(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<GI2DPhysics>& physics_context)
	{
		m_gi2d_context = gi2d_context;
		m_physics_context = physics_context;

		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout[DSL_GameObject] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
		}

		uint size = 1024;
		// descriptor_set
		{
			{
				b_state = context->m_storage_memory.allocateMemory<cState>({ size,{} });
				b_movable = context->m_storage_memory.allocateMemory<cMovable>({ size * 64,{} });				
			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					getDescriptorSetLayout(GameContext::DSL_GameObject),
				};

				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_ds_gameobject = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] =
				{
					b_state.getInfo(),
					b_movable.getInfo(),
				};
				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_ds_gameobject.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);

				m_ds_gameobject_freelist.resize(size);
				for (uint i = 0; i < size; i++)
				{
					m_ds_gameobject_freelist[i].index = i;

				}
			}

		}

	}

	DS_GameObject alloc()
	{
		auto ds = m_ds_gameobject_freelist.front();
		m_ds_gameobject_freelist.pop_front();
		return ds;
	}
	void free(const DS_GameObject& ds)
	{
		m_ds_gameobject_freelist.push_back(ds);
	}
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<GI2DPhysics> m_physics_context;

	btr::BufferMemoryEx<cState> b_state;
	btr::BufferMemoryEx<cMovable> b_movable;


	vk::UniqueDescriptorSet m_ds_gameobject;
	std::deque<DS_GameObject> m_ds_gameobject_freelist;


};
struct GameProcedure
{
	enum Shader
	{
		ShaderPlayer_Move,
		ShaderMovable_UpdatePrePhysics,
		ShaderMovable_UpdatePostPhysics,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_PlayerUpdate,
		PipelineLayout_MovableUpdate,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		PipelinePlayer_Update,
		PipelineMovable_UpdatePrePhysics,
		PipelineMovable_UpdatePostPhysics,
		Pipeline_Num,
	};
	GameProcedure(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GameContext>& game_context)
	{
		{
			const char* name[] =
			{
				"Player_Move.comp.spv",
				"Movable_UpdatePrePhysics.comp.spv",
				"Movable_UpdatePostPhysics.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				game_context->getDescriptorSetLayout(GameContext::DSL_GameObject),
				sSystem::Order().getSystemDescriptorLayout(),
			};

			vk::PushConstantRange ranges[] = {
				vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, 4),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
			pipeline_layout_info.setPPushConstantRanges(ranges);
			m_pipeline_layout[PipelineLayout_PlayerUpdate] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
			vk::DebugUtilsObjectNameInfoEXT name_info;
			name_info.pObjectName = "PipelineLayout_PlayerUpdate";
			name_info.objectType = vk::ObjectType::ePipelineLayout;
			name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_MovableUpdate].get());
			context->m_device->setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
#endif
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				game_context->getDescriptorSetLayout(GameContext::DSL_GameObject),
				game_context->m_physics_context->getDescriptorSetLayout(GI2DPhysics::DescLayout_Data),
			};
			vk::PushConstantRange ranges[] = {
				vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, 4),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
			pipeline_layout_info.setPPushConstantRanges(ranges);
			m_pipeline_layout[PipelineLayout_MovableUpdate] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
			vk::DebugUtilsObjectNameInfoEXT name_info;
			name_info.pObjectName = "PipelineLayout_MovableUpdate";
			name_info.objectType = vk::ObjectType::ePipelineLayout;
			name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_MovableUpdate].get());
			context->m_device->setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
#endif
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
			shader_info[0].setModule(m_shader[ShaderPlayer_Move].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderMovable_UpdatePrePhysics].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[ShaderMovable_UpdatePostPhysics].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_PlayerUpdate].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_MovableUpdate].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_MovableUpdate].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);

#if USE_DEBUG_REPORT
			vk::DebugUtilsObjectNameInfoEXT name_info;
			name_info.pObjectName = "Pipeline_MovableUpdate";
			name_info.objectType = vk::ObjectType::ePipeline;
			name_info.objectHandle = reinterpret_cast<uint64_t &>(compute_pipeline[0].get());
			context->m_device->setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
#endif

			m_pipeline[PipelinePlayer_Update] = std::move(compute_pipeline[0]);
			m_pipeline[PipelineMovable_UpdatePrePhysics] = std::move(compute_pipeline[1]);
			m_pipeline[PipelineMovable_UpdatePostPhysics] = std::move(compute_pipeline[2]);
		}

	}

	void executePlayerUpdate(vk::CommandBuffer cmd, const std::shared_ptr <btr::Context>& context, const std::shared_ptr<GameContext>& game_context, const DS_GameObject& ds)
	{
		DebugLabel _label(cmd, context->m_dispach, __FUNCTION__);

		{
			vk::BufferMemoryBarrier to_read[] = {
				game_context->b_movable.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite|vk::AccessFlagBits::eShaderRead),
			};
			to_read[0].offset += ds.index * sizeof(cMovable);
			to_read[0].size = sizeof(cMovable);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

		}

		vk::DescriptorSet descs[] =
		{
			game_context->m_ds_gameobject.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_PlayerUpdate].get(), 0, array_length(descs), descs, 0, nullptr);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_PlayerUpdate].get(), 1, sSystem::Order().getSystemDescriptorSet(), { 0 });

		{
			cmd.pushConstants<uint>(m_pipeline_layout[PipelineLayout_PlayerUpdate].get(), vk::ShaderStageFlagBits::eCompute, 0, ds.index);
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelinePlayer_Update].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}
	void executeMovableUpdatePrePyhsics(vk::CommandBuffer cmd, const std::shared_ptr <btr::Context>& context, const std::shared_ptr<GameContext>& game_context, const DS_GameObject& ds)
	{
		DebugLabel _label(cmd, context->m_dispach, __FUNCTION__);

		{
			vk::BufferMemoryBarrier to_read[] = {
				game_context->b_movable.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				game_context->m_physics_context->b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			to_read[0].offset += ds.index * sizeof(cMovable);
			to_read[0].size = sizeof(cMovable);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

		}
		vk::DescriptorSet descs[] =
		{
			game_context->m_ds_gameobject.get(),
			game_context->m_physics_context->getDescriptorSet(GI2DPhysics::DescLayout_Data),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MovableUpdate].get(), 0, array_length(descs), descs, 0, nullptr);

		{
			cmd.pushConstants<uint>(m_pipeline_layout[PipelineLayout_MovableUpdate].get(), vk::ShaderStageFlagBits::eCompute, 0, ds.index);
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMovable_UpdatePrePhysics].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}
	void executeMovableUpdatePostPyhsics(vk::CommandBuffer cmd, const std::shared_ptr <btr::Context>& context, const std::shared_ptr<GameContext>& game_context, const DS_GameObject& ds)
	{
		DebugLabel _label(cmd, context->m_dispach, __FUNCTION__);

		{
			vk::BufferMemoryBarrier to_read[] = {
				game_context->b_movable.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			to_read[0].offset += ds.index * sizeof(cMovable);
			to_read[0].size = sizeof(cMovable);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

		}
		vk::DescriptorSet descs[] =
		{
			game_context->m_ds_gameobject.get(),
			game_context->m_physics_context->getDescriptorSet(GI2DPhysics::DescLayout_Data),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MovableUpdate].get(), 0, array_length(descs), descs, 0, nullptr);

		{
			cmd.pushConstants<uint>(m_pipeline_layout[PipelineLayout_MovableUpdate].get(), vk::ShaderStageFlagBits::eCompute, 0, ds.index);
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMovable_UpdatePostPhysics].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
};
