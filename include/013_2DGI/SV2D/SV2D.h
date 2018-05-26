#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
namespace sv2d
{

struct SV2DLightData
{
	vec2 m_pos;
	float m_dir;
	float m_angle;
	vec4 m_emissive;
	int level;
	int _p1;
	int _p2;
	int _p3;
};
struct SV2DContext
{

	int32_t RenderWidth;
	int32_t RenderHeight;
	int FragmentBufferSize;
	struct Info
	{
		mat4 m_camera_PV;
		uvec2 m_resolution;
		uvec2 m_emission_tile_size;
		uvec2 m_emission_tile_num;
		uvec2 _p;
		vec4 m_position;
		int m_emission_tile_linklist_max;
		int __p;
	};

	struct Fragment
	{
		vec3 albedo;
		float _p;
	};

	struct LinkList
	{
		int32_t next;
		int32_t target;
	};

	SV2DContext(const std::shared_ptr<btr::Context>& context)
	{
		RenderWidth = 1024;
		RenderHeight = 1024;
		FragmentBufferSize = RenderWidth * RenderHeight;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			btr::BufferMemoryDescriptorEx<Info> desc;
			desc.element_num = 1;
			u_fragment_info = context->m_uniform_memory.allocateMemory(desc);

			m_sv2d_info.m_position = vec4(0.f, 0.f, 0.f, 0.f);
			m_sv2d_info.m_resolution = uvec2(RenderWidth, RenderHeight);
			m_sv2d_info.m_emission_tile_size = uvec2(32, 32);
			m_sv2d_info.m_emission_tile_num = m_sv2d_info.m_resolution / m_sv2d_info.m_emission_tile_size;
			m_sv2d_info.m_camera_PV = glm::ortho(RenderWidth*-0.5f, RenderWidth*0.5f, RenderHeight*-0.5f, RenderHeight*0.5f, 0.f, 2000.f) * glm::lookAt(vec3(RenderWidth*0.5f, 100.f, RenderHeight*0.5f)+m_sv2d_info.m_position.xyz(), vec3(RenderWidth*0.5f, 0.f, RenderHeight*0.5f) + m_sv2d_info.m_position.xyz(), vec3(0.f, 0.f, 1.f));

			m_sv2d_info.m_emission_tile_linklist_max = 8192 * 1024;
			cmd.updateBuffer<Info>(u_fragment_info.getInfo().buffer, u_fragment_info.getInfo().offset, m_sv2d_info);
		}
		{
			btr::BufferMemoryDescriptorEx<Fragment> desc;
			desc.element_num = FragmentBufferSize;
			b_fragment_buffer = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<ivec4> desc;
			desc.element_num = 1;
			b_emission_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			b_emission_buffer = context->m_storage_memory.allocateMemory<SV2DLightData>({ 2048, {} });
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
			desc.element_num = m_sv2d_info.m_emission_tile_linklist_max;
			b_emission_tile_linklist = context->m_storage_memory.allocateMemory(desc);
		}
		{
			b_shadow_volume = context->m_vertex_memory.allocateMemory<vec2>({ 6 * 1024 * 1024,{} });
			b_shadow_volume_counter = context->m_vertex_memory.allocateMemory<vk::DrawIndirectCommand>({ 1, {} });
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
					.setBinding(30),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(31),
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
				};
				vk::DescriptorBufferInfo emission_storages[] = {
					b_emission_counter.getInfo(),
					b_emission_buffer.getInfo(),
					b_emission_tile_linklist_counter.getInfo(),
					b_emission_tile_linkhead.getInfo(),
					b_emission_tile_linklist.getInfo(),
				};
				vk::DescriptorBufferInfo shadow_storages[] = {
					b_shadow_volume.getInfo(),
					b_shadow_volume_counter.getInfo(),
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
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(shadow_storages))
					.setPBufferInfo(shadow_storages)
					.setDstBinding(30)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}
	}
	Info m_sv2d_info;

	btr::BufferMemoryEx<Info> u_fragment_info;
	btr::BufferMemoryEx<Fragment> b_fragment_buffer;

	btr::BufferMemoryEx<ivec4> b_emission_counter;
	btr::BufferMemoryEx<SV2DLightData> b_emission_buffer;
	btr::BufferMemoryEx<int32_t> b_emission_tile_linklist_counter;
	btr::BufferMemoryEx<int32_t> b_emission_tile_linkhead;
	btr::BufferMemoryEx<LinkList> b_emission_tile_linklist;

	btr::BufferMemoryEx<vec2> b_shadow_volume;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> b_shadow_volume_counter;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};

struct SV2DPipeline
{
	virtual void execute(vk::CommandBuffer cmd) = 0;
};

}
