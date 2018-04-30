#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/PM2D.h>


namespace pm2d
{

struct PM2DContext
{
	enum
	{
		_BounceNum = 4, //!< ƒŒƒC”½ŽË‰ñ”
		_Hierarchy_Num = 8,
	};
	int RenderWidth;
	int RenderHeight;
	int FragmentBufferSize;
	int BounceNum = 4;
	int Hierarchy_Num = 8;
	struct Info
	{
		mat4 m_camera_PV;
		uvec2 m_resolution;
		uvec2 m_emission_tile_size;
		uvec2 m_emission_tile_num;
		uvec2 _p;
		vec4 m_position;
		int m_fragment_hierarchy_offset[_Hierarchy_Num];
		int m_fragment_map_hierarchy_offset[_Hierarchy_Num];
		int m_emission_buffer_size[_BounceNum];
		int m_emission_buffer_offset[_BounceNum];

		int m_emission_tile_linklist_max;
		int m_sdf_work_num;
	};

	struct Fragment
	{
		vec3 albedo;
		float _p;
	};
	struct Emission
	{
		vec4 value;
	};
	struct LinkList
	{
		int32_t next;
		int32_t target;
	};

	PM2DContext(const std::shared_ptr<btr::Context>& context)
	{
		RenderWidth = 1024;
		RenderHeight = 1024;
		FragmentBufferSize = RenderWidth * RenderHeight;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			btr::BufferMemoryDescriptorEx<Info> desc;
			desc.element_num = 1;
			u_fragment_info = context->m_uniform_memory.allocateMemory(desc);

			m_pm2d_info.m_position = vec4(0.f, 0.f, 0.f, 0.f);
			m_pm2d_info.m_resolution = uvec2(RenderWidth, RenderHeight);
			m_pm2d_info.m_emission_tile_size = uvec2(32, 32);
			m_pm2d_info.m_emission_tile_num = m_pm2d_info.m_resolution / m_pm2d_info.m_emission_tile_size;
			m_pm2d_info.m_camera_PV = glm::ortho(-RenderWidth * 0.5f, RenderWidth*0.5f, -RenderHeight * 0.5f, RenderHeight*0.5f);
			m_pm2d_info.m_camera_PV *= glm::lookAt(vec3(0., -1.f, 0.f) + m_pm2d_info.m_position.xyz(), m_pm2d_info.m_position.xyz(), vec3(0.f, 0.f, 1.f));
			m_pm2d_info.m_fragment_hierarchy_offset[0] = 0;
			m_pm2d_info.m_fragment_hierarchy_offset[1] = m_pm2d_info.m_fragment_hierarchy_offset[0] + RenderHeight * RenderWidth;
			m_pm2d_info.m_fragment_hierarchy_offset[2] = m_pm2d_info.m_fragment_hierarchy_offset[1] + RenderHeight * RenderWidth / (2 * 2);
			m_pm2d_info.m_fragment_hierarchy_offset[3] = m_pm2d_info.m_fragment_hierarchy_offset[2] + RenderHeight * RenderWidth / (4 * 4);
			m_pm2d_info.m_fragment_hierarchy_offset[4] = m_pm2d_info.m_fragment_hierarchy_offset[3] + RenderHeight * RenderWidth / (8 * 8);
			m_pm2d_info.m_fragment_hierarchy_offset[5] = m_pm2d_info.m_fragment_hierarchy_offset[4] + RenderHeight * RenderWidth / (16 * 16);
			m_pm2d_info.m_fragment_hierarchy_offset[6] = m_pm2d_info.m_fragment_hierarchy_offset[5] + RenderHeight * RenderWidth / (32 * 32);
			m_pm2d_info.m_fragment_hierarchy_offset[7] = m_pm2d_info.m_fragment_hierarchy_offset[6] + RenderHeight * RenderWidth / (64 * 64);

			int size = RenderHeight * RenderWidth / 64;
			m_pm2d_info.m_fragment_map_hierarchy_offset[0] = 0;
			m_pm2d_info.m_fragment_map_hierarchy_offset[1] = m_pm2d_info.m_fragment_map_hierarchy_offset[0] + size;
			m_pm2d_info.m_fragment_map_hierarchy_offset[2] = m_pm2d_info.m_fragment_map_hierarchy_offset[1] + size / (2 * 2);
			m_pm2d_info.m_fragment_map_hierarchy_offset[3] = m_pm2d_info.m_fragment_map_hierarchy_offset[2] + size / (4 * 4);
			m_pm2d_info.m_fragment_map_hierarchy_offset[4] = m_pm2d_info.m_fragment_map_hierarchy_offset[3] + size / (8 * 8);
			m_pm2d_info.m_fragment_map_hierarchy_offset[5] = m_pm2d_info.m_fragment_map_hierarchy_offset[4] + size / (16 * 16);
			m_pm2d_info.m_fragment_map_hierarchy_offset[6] = m_pm2d_info.m_fragment_map_hierarchy_offset[5] + size / (32 * 32);
			m_pm2d_info.m_fragment_map_hierarchy_offset[7] = m_pm2d_info.m_fragment_map_hierarchy_offset[6] + size / (64 * 64);

			m_pm2d_info.m_emission_buffer_size[0] = RenderWidth * RenderHeight;
			m_pm2d_info.m_emission_buffer_offset[0] = 0;
			for (int i = 1; i < BounceNum; i++)
			{
				m_pm2d_info.m_emission_buffer_size[i] = m_pm2d_info.m_emission_buffer_size[i - 1] / 4;
				m_pm2d_info.m_emission_buffer_offset[i] = m_pm2d_info.m_emission_buffer_offset[i - 1] + m_pm2d_info.m_emission_buffer_size[i - 1];
			}
			m_pm2d_info.m_emission_tile_linklist_max = 8192 * 1024;
			m_pm2d_info.m_sdf_work_num = RenderWidth * RenderHeight * 30;
			cmd.updateBuffer<Info>(u_fragment_info.getInfo().buffer, u_fragment_info.getInfo().offset, m_pm2d_info);
		}
		{
			btr::BufferMemoryDescriptorEx<Fragment> desc;
			desc.element_num = FragmentBufferSize;
			b_fragment_buffer = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int64_t> desc;
			int size = RenderHeight * RenderWidth / 64;
			desc.element_num = size;
			desc.element_num += size / (2 * 2);
			desc.element_num += size / (4 * 4);
			desc.element_num += size / (8 * 8);
			desc.element_num += size / (16 * 16);
			desc.element_num += size / (32 * 32);
			desc.element_num += size / (64 * 64);
			desc.element_num += size / (128 * 128);
			b_fragment_map = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = RenderWidth * RenderHeight;
			desc.element_num += RenderWidth * RenderHeight / (2 * 2);
			desc.element_num += RenderWidth * RenderHeight / (4 * 4);
			desc.element_num += RenderWidth * RenderHeight / (8 * 8);
			desc.element_num += RenderWidth * RenderHeight / (16 * 16);
			desc.element_num += RenderWidth * RenderHeight / (32 * 32);
			desc.element_num += RenderWidth * RenderHeight / (64 * 64);
			desc.element_num += RenderWidth * RenderHeight / (128 * 128);
			b_fragment_hierarchy = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<ivec4> desc;
			desc.element_num = BounceNum;
			b_emission_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<Emission> desc;
			desc.element_num = m_pm2d_info.m_emission_buffer_offset[BounceNum - 1] + m_pm2d_info.m_emission_buffer_size[BounceNum - 1];
			b_emission_buffer = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = m_pm2d_info.m_emission_buffer_offset[BounceNum - 1] + m_pm2d_info.m_emission_buffer_size[BounceNum - 1];
			b_emission_list = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = m_pm2d_info.m_emission_buffer_offset[BounceNum - 1] + m_pm2d_info.m_emission_buffer_size[BounceNum - 1];
			b_emission_map = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = 1;
			b_emission_tile_linklist_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = RenderHeight * RenderWidth;
			b_emission_tile_linkhead = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<LinkList> desc;
			desc.element_num = m_pm2d_info.m_emission_tile_linklist_max;
			b_emission_tile_linklist = context->m_storage_memory.allocateMemory(desc);
		}

		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(1),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(2),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(3),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(4),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(5),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(6),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(7),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(8),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(9),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(20),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(21),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(22),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(23),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(24),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(25),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(26),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo uniforms[] = {
					u_fragment_info.getInfo(),
				};
				vk::DescriptorBufferInfo storages[] = {
					b_fragment_buffer.getInfo(),
					b_fragment_map.getInfo(),
					b_fragment_hierarchy.getInfo(),
				};
				vk::DescriptorBufferInfo emission_storages[] = {
					b_emission_counter.getInfo(),
					b_emission_buffer.getInfo(),
					b_emission_list.getInfo(),
					b_emission_map.getInfo(),
					b_emission_tile_linklist_counter.getInfo(),
					b_emission_tile_linkhead.getInfo(),
					b_emission_tile_linklist.getInfo(),
				};

				vk::WriteDescriptorSet write[] = {
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(array_length(uniforms))
					.setPBufferInfo(uniforms)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(1)
					.setDstSet(m_descriptor_set.get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(emission_storages))
					.setPBufferInfo(emission_storages)
					.setDstBinding(20)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}
	}
	Info m_pm2d_info;

	btr::BufferMemoryEx<Info> u_fragment_info;
	btr::BufferMemoryEx<Fragment> b_fragment_buffer;
	btr::BufferMemoryEx<int64_t> b_fragment_map;
	btr::BufferMemoryEx<int32_t> b_fragment_hierarchy;
	btr::BufferMemoryEx<ivec4> b_emission_counter;
	btr::BufferMemoryEx<Emission> b_emission_buffer;

	btr::BufferMemoryEx<int32_t> b_emission_list;
	btr::BufferMemoryEx<int32_t> b_emission_map;	//!< ==-1 emitter‚ª‚È‚¢ !=0‚ ‚é
	btr::BufferMemoryEx<int32_t> b_emission_tile_linklist_counter;
	btr::BufferMemoryEx<int32_t> b_emission_tile_linkhead;
	btr::BufferMemoryEx<LinkList> b_emission_tile_linklist;
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};

struct PM2DClear
{
	PM2DClear(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context)
	{
		m_context = context;
		m_pm2d_context = pm2d_context;
	}
	void execute(vk::CommandBuffer cmd)
	{
		{
			// clear
			cmd.fillBuffer(m_pm2d_context->b_fragment_buffer.getInfo().buffer, m_pm2d_context->b_fragment_buffer.getInfo().offset, m_pm2d_context->b_fragment_buffer.getInfo().range, 0u);

			ivec4 emissive[] = { { 0,1,1,0 },{ 0,1,1,0 },{ 0,1,1,0 },{ 0,1,1,0 } };
			static_assert(array_length(emissive) == PM2DContext::_BounceNum, "");
			cmd.updateBuffer(m_pm2d_context->b_emission_counter.getInfo().buffer, m_pm2d_context->b_emission_counter.getInfo().offset, sizeof(emissive), emissive);
			cmd.fillBuffer(m_pm2d_context->b_emission_buffer.getInfo().buffer, m_pm2d_context->b_emission_buffer.getInfo().offset, m_pm2d_context->b_emission_buffer.getInfo().range, 0);
			cmd.fillBuffer(m_pm2d_context->b_emission_list.getInfo().buffer, m_pm2d_context->b_emission_list.getInfo().offset, m_pm2d_context->b_emission_list.getInfo().range, -1);
			cmd.fillBuffer(m_pm2d_context->b_emission_map.getInfo().buffer, m_pm2d_context->b_emission_map.getInfo().offset, m_pm2d_context->b_emission_map.getInfo().range, -1);
		}
	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;
};

struct PM2DMakeHierarchy
{
	enum Shader
	{
		ShaderMakeFragmentMap,
		ShaderMakeFragmentMapHierarchy,
		ShaderMakeFragmentHierarchy,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakeFragmentMap,
		PipelineLayoutMakeFragmentMapHierarchy,
		PipelineLayoutMakeFragmentHierarchy,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakeFragmentMap,
		PipelineMakeFragmentMapHierarchy,
		PipelineMakeFragmentHierarchy,
		PipelineNum,
	};

	PM2DMakeHierarchy(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context)
	{
		m_context = context;
		m_pm2d_context = pm2d_context;

		{
			const char* name[] =
			{
				"MakeFragmentMap.comp.spv",
				"MakeFragmentMapHierarchy.comp.spv",
				"MakeFragmentHierarchy.comp.spv",
			};
//			static_assert(array_length(name) == m_shader.max_size(), "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				pm2d_context->getDescriptorSetLayout()
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayoutMakeFragmentMap] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 3> shader_info;
			shader_info[0].setModule(m_shader[ShaderMakeFragmentMap].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderMakeFragmentMapHierarchy].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[ShaderMakeFragmentHierarchy].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentMap].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PipelineMakeFragmentMap] = std::move(compute_pipeline[0]);
			m_pipeline[PipelineMakeFragmentMapHierarchy] = std::move(compute_pipeline[1]);
			m_pipeline[PipelineMakeFragmentHierarchy] = std::move(compute_pipeline[2]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{
		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_pm2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_pm2d_context->b_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentMap].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentMap].get(), 0, m_pm2d_context->getDescriptorSet(), {});

			auto num = app::calcDipatchGroups(uvec3(m_pm2d_context->RenderWidth, m_pm2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// make map_hierarchy
		{
			for (int i = 1; i < PM2DContext::_Hierarchy_Num; i++)
			{
				vk::BufferMemoryBarrier to_write[] = {
					m_pm2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentMapHierarchy].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get(), 0, m_pm2d_context->getDescriptorSet(), {});
				cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayoutMakeFragmentMapHierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);

				auto num = app::calcDipatchGroups(uvec3(m_pm2d_context->RenderWidth >> i, m_pm2d_context->RenderHeight >> i, 1), uvec3(32, 32, 1));
				cmd.dispatch(num.x, num.y, num.z);

			}
		}
		// make hierarchy
		{
			for (int i = 1; i < PM2DContext::_Hierarchy_Num; i++)
			{
				vk::BufferMemoryBarrier to_write[] = {
					m_pm2d_context->b_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_write), to_write, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentHierarchy].get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get(), 0, m_pm2d_context->getDescriptorSet(), {});
				cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);

				auto num = app::calcDipatchGroups(uvec3((m_pm2d_context->RenderWidth >> (i - 1)), (m_pm2d_context->RenderHeight >> (i - 1)), 1), uvec3(32, 32, 1));
				cmd.dispatch(num.x, num.y, num.z);

			}
		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

};


struct PM2DRenderer
{

	enum Shader
	{
		ShaderLightCulling,
		ShaderPhotonMapping,
		ShaderRenderingVS,
		ShaderRenderingFS,
		ShaderDebugRenderFragmentMap,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutLightCulling,
		PipelineLayoutPhotonMapping,
		PipelineLayoutRendering,
		PipelineLayoutDebugRenderFragmentMap,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineLightCulling,
		PipelinePhotonMapping,
		PipelineRendering,
		PipelineDebugRenderFragmentMap,
		PipelineNum,
	};

	struct TextureResource
	{
		vk::ImageCreateInfo m_image_info;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;

		vk::ImageSubresourceRange m_subresource_range;
	};

	PM2DRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target, const std::shared_ptr<PM2DContext>& pm2d_context);
	void execute(vk::CommandBuffer cmd);

	std::array < TextureResource, PM2DContext::_BounceNum > m_color_tex;


	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueShaderModule m_shader[ShaderNum];
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;
private:
	void DebugRnederFragmentMap(PM2DContext* pm2d_context, vk::CommandBuffer &cmd);
};

struct AppModelPM2D : public PM2DPipeline
{
	AppModelPM2D(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DRenderer>& renderer);
	void execute(vk::CommandBuffer cmd)override
	{

	}
	vk::UniqueShaderModule m_shader[2];
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;
};

struct PM2DDebug : public PM2DPipeline
{
	PM2DDebug(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context);
	void execute(vk::CommandBuffer cmd) override;
		
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;

	std::vector<PM2DContext::Emission> m_emission;
	btr::BufferMemoryEx<PM2DContext::Fragment> m_map_data;

	enum Shader
	{
		ShaderPointLight,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutPointLight,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelinePointLight,
		PipelineNum,
	};
	vk::UniqueShaderModule m_shader[ShaderNum];
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

};

}