#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
namespace gi2d
{


struct GI2DContext
{
	enum
	{
		_BounceNum = 4, //!< ƒŒƒC”½ŽË‰ñ”
		_Light_Num = 256,
	};
	int32_t RenderWidth;
	int32_t RenderHeight;
	ivec2 RenderSize;
	int FragmentBufferSize;
	int BounceNum = _BounceNum;
	int Light_Num = _Light_Num;
	struct GI2DInfo
	{
		mat4 m_camera_PV;
		uvec2 m_resolution;
		uvec2 _p;
		vec4 m_position;
	};
	struct GI2DScene
	{
		int32_t m_frame;
		int32_t m_hierarchy;
		uint32_t m_skip;
		int32_t _p2;

		uint32_t m_radiance_offset;
		uint32_t m_map_offset;
		uvec2 m_map_reso;
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

	// https://postd.cc/voronoi-diagrams/
	struct D2JFACell
	{
		ivec2 nearest_index;
		float distance;
		int _p;
		ivec2 e_nearest_index;
		float e_distance;
		int _ep;

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
			m_gi2d_info.m_camera_PV = glm::ortho(RenderWidth*-0.5f, RenderWidth*0.5f, RenderHeight*-0.5f, RenderHeight*0.5f, 0.f, 2000.f) * glm::lookAt(vec3(RenderWidth*0.5f, -1000.f, RenderHeight*0.5f)+m_gi2d_info.m_position.xyz(), vec3(RenderWidth*0.5f, 0.f, RenderHeight*0.5f) + m_gi2d_info.m_position.xyz(), vec3(0.f, 0.f, 1.f));

			cmd.updateBuffer<GI2DInfo>(u_gi2d_info.getInfo().buffer, u_gi2d_info.getInfo().offset, m_gi2d_info);

			m_gi2d_scene.m_frame = 0;
			m_gi2d_scene.m_hierarchy = 0;
			m_gi2d_scene.m_skip = 0;
			cmd.updateBuffer<GI2DScene>(u_gi2d_scene.getInfo().buffer, u_gi2d_scene.getInfo().offset, m_gi2d_scene);
		}
		{
			b_fragment = context->m_storage_memory.allocateMemory<Fragment>({ FragmentBufferSize, {} });
			b_fragment_map = context->m_storage_memory.allocateMemory<u64vec2>({ FragmentBufferSize / 64, {} });
			b_diffuse_map = context->m_storage_memory.allocateMemory<uint64_t>({ FragmentBufferSize / 64, {} });
			b_emissive_map = context->m_storage_memory.allocateMemory<uint64_t>({ FragmentBufferSize / 64, {} });
			b_light = context->m_storage_memory.allocateMemory<uint32_t>({ FragmentBufferSize, {} });
			b_grid_counter = context->m_storage_memory.allocateMemory<int32_t>({ FragmentBufferSize,{} });
			b_jfa = context->m_storage_memory.allocateMemory<D2JFACell>({ FragmentBufferSize,{} });
			b_sdf = context->m_storage_memory.allocateMemory<vec2>({ FragmentBufferSize,{} });
		}

		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
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
					b_fragment_map.getInfo(),
					b_grid_counter.getInfo(),
					b_light.getInfo(),
					b_jfa.getInfo(),
					b_sdf.getInfo(),
					b_diffuse_map.getInfo(),
					b_emissive_map.getInfo(),
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
		m_gi2d_scene.m_frame = (m_gi2d_scene.m_frame + 1) % 4;
		auto reso = uvec4(m_gi2d_info.m_resolution, m_gi2d_info.m_resolution/8);

		uint radiance_offset = reso.x*reso.y;
		int map_offset = 0;
		uvec2 map_reso = reso.zw();
		for (int i = 0; i < m_gi2d_scene.m_hierarchy; i++)
		{
			radiance_offset >>= 2;
			map_offset += (reso.z >> i)*(reso.w >> i);
			map_reso >>= 1;
		}
		m_gi2d_scene.m_radiance_offset = radiance_offset;
		m_gi2d_scene.m_map_offset = map_offset;
		m_gi2d_scene.m_map_reso = map_reso;

		{
			vk::BufferMemoryBarrier to_write[] = {
				u_gi2d_scene.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);
		}

		cmd.updateBuffer<GI2DScene>(u_gi2d_scene.getInfo().buffer, u_gi2d_scene.getInfo().offset, m_gi2d_scene);

		{
			vk::BufferMemoryBarrier to_read[] = {
				u_gi2d_scene.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
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
	btr::BufferMemoryEx<u64vec2> b_fragment_map;
	btr::BufferMemoryEx<uint64_t> b_diffuse_map;
	btr::BufferMemoryEx<uint64_t> b_emissive_map;
	btr::BufferMemoryEx<int32_t> b_grid_counter;
	btr::BufferMemoryEx<uint32_t> b_light;
	btr::BufferMemoryEx<D2JFACell> b_jfa;
	btr::BufferMemoryEx<vec2> b_sdf;

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
