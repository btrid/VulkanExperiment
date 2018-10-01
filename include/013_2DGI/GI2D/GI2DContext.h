#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
namespace gi2d
{

struct GI2DLightData
{
	vec4 m_pos;
	vec4 m_emissive;
};
struct GI2DContext
{
	enum
	{
		_BounceNum = 4, //!< ƒŒƒC”½ŽË‰ñ”
		_Hierarchy_Num = 8,
		_Light_Num = 256,
	};
	int32_t RenderWidth;
	int32_t RenderHeight;
	ivec2 RenderSize;
	int FragmentBufferSize;
	int BounceNum = _BounceNum;
	int Hierarchy_Num = _Hierarchy_Num;
	int Light_Num = _Light_Num;
	struct GI2DInfo
	{
		mat4 m_camera_PV;
		uvec2 m_resolution;
		uvec2 m_emission_tile_size;
		uvec2 m_emission_tile_num;
		uvec2 _p;
		vec4 m_position;
		int m_fragment_map_hierarchy_offset[_Hierarchy_Num];

		int m_emission_tile_linklist_max;
		int m_emission_buffer_max;
	};
	struct GI2DScene
	{
		int32_t m_frame;
		int32_t m_hierarchy;
	};

	struct Fragment
	{
		vec4 albedo;
	};

	struct LinkList
	{
		int32_t next;
		int32_t target;
	};

	GI2DContext(const std::shared_ptr<btr::Context>& context)
	{
 		RenderWidth = 1024;
 		RenderHeight = 1024;
		RenderSize = ivec2(RenderWidth, RenderHeight);
		FragmentBufferSize = RenderWidth * RenderHeight;


		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			u_gi2d_info = context->m_uniform_memory.allocateMemory<GI2DInfo>({ 1, {} });
			u_gi2d_scene = context->m_uniform_memory.allocateMemory<GI2DScene>({ 1, {} });

			m_gi2d_info.m_position = vec4(0.f, 0.f, 0.f, 0.f);
			m_gi2d_info.m_resolution = uvec2(RenderWidth, RenderHeight);
			m_gi2d_info.m_emission_tile_size = uvec2(32, 32);
			m_gi2d_info.m_emission_tile_num = m_gi2d_info.m_resolution / m_gi2d_info.m_emission_tile_size;
			m_gi2d_info.m_camera_PV = glm::ortho(RenderWidth*-0.5f, RenderWidth*0.5f, RenderHeight*-0.5f, RenderHeight*0.5f, 0.f, 2000.f) * glm::lookAt(vec3(RenderWidth*0.5f, -1000.f, RenderHeight*0.5f)+m_gi2d_info.m_position.xyz(), vec3(RenderWidth*0.5f, 0.f, RenderHeight*0.5f) + m_gi2d_info.m_position.xyz(), vec3(0.f, 0.f, 1.f));

			int size = RenderHeight * RenderWidth / 64;
			m_gi2d_info.m_fragment_map_hierarchy_offset[0] = 0;
			m_gi2d_info.m_fragment_map_hierarchy_offset[1] = m_gi2d_info.m_fragment_map_hierarchy_offset[0] + size;
			m_gi2d_info.m_fragment_map_hierarchy_offset[2] = m_gi2d_info.m_fragment_map_hierarchy_offset[1] + size / (2 * 2);
			m_gi2d_info.m_fragment_map_hierarchy_offset[3] = m_gi2d_info.m_fragment_map_hierarchy_offset[2] + size / (4 * 4);
			m_gi2d_info.m_fragment_map_hierarchy_offset[4] = m_gi2d_info.m_fragment_map_hierarchy_offset[3] + size / (8 * 8);
			m_gi2d_info.m_fragment_map_hierarchy_offset[5] = m_gi2d_info.m_fragment_map_hierarchy_offset[4] + size / (16 * 16);
			m_gi2d_info.m_fragment_map_hierarchy_offset[6] = m_gi2d_info.m_fragment_map_hierarchy_offset[5] + size / (32 * 32);
			m_gi2d_info.m_fragment_map_hierarchy_offset[7] = m_gi2d_info.m_fragment_map_hierarchy_offset[6] + size / (64 * 64);

			m_gi2d_info.m_emission_tile_linklist_max = Light_Num * m_gi2d_info.m_emission_tile_num.x * m_gi2d_info.m_emission_tile_num.y;
			m_gi2d_info.m_emission_buffer_max = Light_Num;
			cmd.updateBuffer<GI2DInfo>(u_gi2d_info.getInfo().buffer, u_gi2d_info.getInfo().offset, m_gi2d_info);

			m_gi2d_scene.m_frame = 0;
			m_gi2d_scene.m_hierarchy = 0;
			cmd.updateBuffer<GI2DScene>(u_gi2d_scene.getInfo().buffer, u_gi2d_scene.getInfo().offset, m_gi2d_scene);
		}
		{
			b_fragment = context->m_storage_memory.allocateMemory<Fragment>({ FragmentBufferSize , {} });
			b_light_counter = context->m_storage_memory.allocateMemory<uvec4>({1, {} });
			b_light_index = context->m_storage_memory.allocateMemory<uint32_t>({ FragmentBufferSize, {} });
			b_light = context->m_storage_memory.allocateMemory<uint32_t>({ FragmentBufferSize * 2, {} });
		}
		{
			btr::BufferMemoryDescriptorEx<uint64_t> desc;
			int size = RenderHeight * RenderWidth / 64;
			desc.element_num = size;
			desc.element_num += size / (2 * 2);
			desc.element_num += size / (4 * 4);
			desc.element_num += size / (8 * 8);
			desc.element_num += size / (16 * 16);
			desc.element_num += size / (32 * 32);
			desc.element_num += size / (64 * 64);
			desc.element_num += size / (128 * 128);
			b_diffuse_map = context->m_storage_memory.allocateMemory(desc);
			b_emissive_map = context->m_storage_memory.allocateMemory(desc);
		}

		uint32_t size = 0;
		for (int i = 0; i < _Hierarchy_Num; i++)
		{
			size += (RenderWidth>>i) * (RenderHeight>>i);
		}
		b_grid_counter = context->m_storage_memory.allocateMemory<int32_t>( {size,{} });

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
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
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
					u_gi2d_info.getInfo(),
					u_gi2d_scene.getInfo(),
				};
				vk::DescriptorBufferInfo storages[] = {
					b_fragment.getInfo(),
					b_diffuse_map.getInfo(),
					b_emissive_map.getInfo(),
					b_grid_counter.getInfo(),
					b_light.getInfo(),
					b_light_counter.getInfo(),
					b_light_index.getInfo(),
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
					.setDstBinding(2)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}
	}

	void execute(vk::CommandBuffer cmd)
	{
//		m_gi2d_scene.m_frame = (m_gi2d_scene.m_frame+1) % 4;

		{
			vk::BufferMemoryBarrier to_write[] = {
				u_gi2d_scene.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				b_light_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);
		}

		cmd.updateBuffer<GI2DScene>(u_gi2d_scene.getInfo().buffer, u_gi2d_scene.getInfo().offset, m_gi2d_scene);
		cmd.updateBuffer<uvec4>(b_light_counter.getInfo().buffer, b_light_counter.getInfo().offset, uvec4(0, 1, 1, 0));

		{
			vk::BufferMemoryBarrier to_read[] = {
				u_gi2d_scene.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_light_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}
	}
	GI2DInfo m_gi2d_info;
	GI2DScene m_gi2d_scene;

	btr::BufferMemoryEx<GI2DInfo> u_gi2d_info;
	btr::BufferMemoryEx<GI2DScene> u_gi2d_scene;
	btr::BufferMemoryEx<Fragment> b_fragment;
	btr::BufferMemoryEx<uint64_t> b_diffuse_map;
	btr::BufferMemoryEx<uint64_t> b_emissive_map;
	btr::BufferMemoryEx<int32_t> b_grid_counter;
	btr::BufferMemoryEx<uint32_t> b_light;
	btr::BufferMemoryEx<uvec4> b_light_counter;
	btr::BufferMemoryEx<uint32_t> b_light_index;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};

struct GI2DPipeline
{
	virtual void execute(vk::CommandBuffer cmd) = 0;
};

}
