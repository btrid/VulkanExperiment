#pragma once

#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <013_2DGI/GI2D/GI2DContext.h>
#include <013_2DGI/GI2D/GI2DPhysics.h>


struct cMemoryManager
{
	uint block_max;
	uint active_index;
	uint free_index;
	uint allocate_address;
};

struct cResourceAccessorInfo
{
	uint stride;
};
struct cResourceAccessor
{
	uint gameobject_address;
	uint rb_address;
};

struct cMovable
{
	vec2 pos;
	vec2 dir;
	float scale;
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
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout[DSL_GameObject] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
		}

		uint size = 1024;
		{
			{
				b_memory_manager = context->m_storage_memory.allocateMemory<cMemoryManager>({ 1,{} });
				b_memory_list = context->m_storage_memory.allocateMemory<uint>({ size,{} });
				b_accessor_info = context->m_storage_memory.allocateMemory<cResourceAccessorInfo>({ 1,{} });
				b_accessor = context->m_storage_memory.allocateMemory<cResourceAccessor>({ size,{} });
				b_movable = context->m_storage_memory.allocateMemory<cMovable>({ size * 64,{} });
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

				cmd.updateBuffer<cResourceAccessorInfo>(b_accessor_info.getInfo().buffer, b_accessor_info.getInfo().offset, cResourceAccessorInfo{ sizeof(cResourceAccessor) / 4 });
				cmd.updateBuffer<cResourceAccessor>(b_accessor.getInfo().buffer, b_accessor.getInfo().offset, cResourceAccessor{ -1, -1 });
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
					b_memory_manager.getInfo(),
					b_memory_list.getInfo(),
					b_accessor_info.getInfo(),
					b_accessor.getInfo(),
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

			}

		}

		{
			std::array<vk::DescriptorPoolSize, 1> pool_size;
			pool_size[0].setType(vk::DescriptorType::eStorageBuffer);
			pool_size[0].setDescriptorCount(size * 6);

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount(array_length(pool_size));
			pool_info.setPPoolSizes(pool_size.data());
			pool_info.setMaxSets(size);
			m_dp_gameobject = context->m_device->createDescriptorPoolUnique(pool_info);

			vk::DescriptorSetLayout layouts[] =
			{
				getDescriptorSetLayout(GameContext::DSL_GameObject),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(m_dp_gameobject.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);

			for (uint i = 0; i<size; i++)
			{
				m_ds_gameobject_holder[i] = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);
				m_ds_gameobject_freelist[i] = m_ds_gameobject_holder[i].get();
				auto info = b_accessor.getInfo();
				info.offset += i * sizeof(cResourceAccessor);
				info.range = sizeof(cResourceAccessor);
				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setPBufferInfo(&info)
					.setDstBinding(0)
					.setDstSet(m_ds_gameobject_holder[i].get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}
	}

	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<GI2DPhysics> m_physics_context;

	btr::BufferMemoryEx<cMemoryManager> b_memory_manager;
	btr::BufferMemoryEx<uint> b_memory_list;
	btr::BufferMemoryEx<cResourceAccessorInfo> b_accessor_info;
	btr::BufferMemoryEx<cResourceAccessor> b_accessor;
	btr::BufferMemoryEx<cMovable> b_movable;
	vk::UniqueDescriptorSet m_ds_gameobject;

	vk::UniqueDescriptorPool m_dp_gameobject;
	std::vector<vk::UniqueDescriptorSet> m_ds_gameobject_holder;
	std::deque<vk::DescriptorSet> m_ds_gameobject_freelist;

};
struct GameProcedure
{
	enum Shader
	{
		ShaderMovable_UpdatePrePhysics,
		ShaderMovable_UpdatePostPhysics,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_MovableUpdate,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		PipelineMovable_UpdatePrePhysics,
		PipelineMovable_UpdatePostPhysics,
		Pipeline_Num,
	};
	GameProcedure(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GameContext>& game_context)
	{
		{
			const char* name[] =
			{
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
				game_context->getDescriptorSetLayout(GameContext::DSL_Movable),
				game_context->m_physics_context->getDescriptorSetLayout(GI2DPhysics::DescLayout_Data),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
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
			shader_info[0].setModule(m_shader[ShaderMovable_UpdatePrePhysics].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderMovable_UpdatePostPhysics].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_MovableUpdate].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
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

			m_pipeline[PipelineMovable_UpdatePrePhysics] = std::move(compute_pipeline[0]);
			m_pipeline[PipelineMovable_UpdatePostPhysics] = std::move(compute_pipeline[1]);
		}

	}

	void executeMovableUpdatePrePyhsics(vk::CommandBuffer cmd, const std::shared_ptr <btr::Context>& context, const std::shared_ptr<GameContext>& game_context, vk::DescriptorSet desc)
	{
		DebugLabel _label(cmd, context->m_dispach, __FUNCTION__);

		vk::DescriptorSet descs[] =
		{
			desc,
			//			game_context->physics_data->getDescriptorSet(GI2DPhysics::DescLayout_Data),
			//			physics_data->getDescriptorSet(GI2DPhysics::DescLayout_Data),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MovableUpdate].get(), 0, array_length(descs), descs, 0, nullptr);

		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[ShaderMovable_UpdatePrePhysics].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	void executeMovableUpdatePostPyhsics(vk::CommandBuffer cmd, const std::shared_ptr <btr::Context>& context, const std::shared_ptr<GameContext>& game_context, vk::DescriptorSet desc)
	{
		DebugLabel _label(cmd, context->m_dispach, __FUNCTION__);

		vk::DescriptorSet descs[] =
		{
			desc,
			//			physics_data->getDescriptorSet(GI2DPhysics::DescLayout_Data),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MovableUpdate].get(), 0, array_length(descs), descs, 0, nullptr);

		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[ShaderMovable_UpdatePostPhysics].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
};
