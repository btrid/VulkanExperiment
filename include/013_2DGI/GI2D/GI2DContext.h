#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>

struct GI2DDescription
{
	int32_t RenderWidth;
	int32_t RenderHeight;
};
struct GI2DContext
{
	enum Layout
	{
		Layout_Data,
		Layout_SDF,
		Layout_Num,
	};
	int32_t RenderWidth;
	int32_t RenderHeight;
	ivec2 RenderSize;
	int FragmentBufferSize;
	struct GI2DInfo
	{
		mat4 m_camera_PV;
		uvec4 m_resolution;
		vec4 m_position;
		std::array<uint32_t, 4> m_fragment_map_size_hierarchy;
		uint32_t m_hierarchy_num;

		uint32_t  getsize(int32_t hierarchy) {
			return m_fragment_map_size_hierarchy[hierarchy] - m_fragment_map_size_hierarchy[hierarchy-1];
		}
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
		uint32_t data : 30;
		uint32_t is_diffuse : 1;
		uint32_t is_emissive : 1;
		Fragment()
		{}
		Fragment(const vec3& rgb, bool d, bool e)
		{
			data = packRGB(rgb);
			is_diffuse = d ? 1 : 0;
			is_emissive = e ? 1 : 0;
		}
#define _maxf (1023.f)
#define _maxi (1023)
		static uint packRGB(const vec3& rgb)
		{
			ivec3 irgb = ivec3(rgb*_maxf);
			irgb = glm::min(irgb, ivec3(_maxi));
			irgb <<= ivec3(20, 10, 0);
			return irgb.x | irgb.y | irgb.z;
		}
		static vec3 unpackRGB(uint irgb)
		{
			vec3 rgb = vec3((uvec3(irgb) >> uvec3(20, 10, 0)) & ((uvec3(1) << uvec3(10, 10, 10)) - uvec3(1)));
			return rgb / _maxf;
		}

		void setRGB(const vec3& rgb)
		{
			data = packRGB(rgb);
		}
		vec3 getRGB()const
		{
			return unpackRGB(data);
		}
	};

	struct LinkList
	{
		int32_t next;
		int32_t target;
	};

	GI2DContext(const std::shared_ptr<btr::Context>& context, const GI2DDescription& desc)
	{
 		RenderWidth = desc.RenderHeight;
 		RenderHeight = desc.RenderWidth;
		RenderSize = ivec2(RenderWidth, RenderHeight);
		FragmentBufferSize = RenderWidth * RenderHeight;

		m_context = context;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			u_gi2d_info = context->m_uniform_memory.allocateMemory<GI2DInfo>({ 1, {} });
			u_gi2d_scene = context->m_uniform_memory.allocateMemory<GI2DScene>({ 1, {} });

			m_gi2d_info.m_position = vec4(0.f, 0.f, 0.f, 0.f);
			m_gi2d_info.m_resolution = uvec4(RenderWidth, RenderHeight, RenderWidth/8, RenderHeight/8);
			m_gi2d_info.m_camera_PV = glm::ortho(RenderWidth*-0.5f, RenderWidth*0.5f, RenderHeight*-0.5f, RenderHeight*0.5f, 0.f, 2000.f) * glm::lookAt(vec3(RenderWidth*0.5f, 1000.f, RenderHeight*0.5f) + m_gi2d_info.m_position.xyz(), vec3(RenderWidth*0.5f, 0.f, RenderHeight*0.5f) + m_gi2d_info.m_position.xyz(), vec3(0.f, 0.f, 1.f));
			m_gi2d_info.m_hierarchy_num = 4;

			int size = RenderHeight * RenderWidth / 64;
			m_gi2d_info.m_fragment_map_size_hierarchy[0] = size;
			size /= 64;
			m_gi2d_info.m_fragment_map_size_hierarchy[1] = m_gi2d_info.m_fragment_map_size_hierarchy[0] + size;
			size /= 64;
			m_gi2d_info.m_fragment_map_size_hierarchy[2] = m_gi2d_info.m_fragment_map_size_hierarchy[1] + size;
			size /= 64;
			m_gi2d_info.m_fragment_map_size_hierarchy[3] = m_gi2d_info.m_fragment_map_size_hierarchy[2] + glm::max(size,1);

			cmd.updateBuffer<GI2DInfo>(u_gi2d_info.getInfo().buffer, u_gi2d_info.getInfo().offset, m_gi2d_info);

			m_gi2d_scene.m_frame = 0;
			m_gi2d_scene.m_hierarchy = 0;
			m_gi2d_scene.m_skip = 0;
			cmd.updateBuffer<GI2DScene>(u_gi2d_scene.getInfo().buffer, u_gi2d_scene.getInfo().offset, m_gi2d_scene);
		}
		{
			b_fragment = context->m_storage_memory.allocateMemory<Fragment>({ FragmentBufferSize, {} });
			b_fragment_map = context->m_storage_memory.allocateMemory<u64vec2>({ m_gi2d_info.m_fragment_map_size_hierarchy[m_gi2d_info.m_hierarchy_num-1], {} });
			b_diffuse_map = context->m_storage_memory.allocateMemory<uint64_t>({ m_gi2d_info.m_fragment_map_size_hierarchy[m_gi2d_info.m_hierarchy_num - 1], {} });
			b_emissive_map = context->m_storage_memory.allocateMemory<uint64_t>({ m_gi2d_info.m_fragment_map_size_hierarchy[m_gi2d_info.m_hierarchy_num - 1], {} });
			b_light = context->m_storage_memory.allocateMemory<uint32_t>({ FragmentBufferSize, {} });
			b_grid_counter = context->m_storage_memory.allocateMemory<int32_t>({ FragmentBufferSize,{} });
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
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout[Layout_Data] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout[Layout_SDF] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}

			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout[Layout_Data].get(),
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
		auto reso = m_gi2d_info.m_resolution;

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
	std::shared_ptr<btr::Context> m_context;
	
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

	std::array<vk::UniqueDescriptorSetLayout, Layout_Num> m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(Layout i)const { return m_descriptor_set_layout[i].get(); }
};

struct GI2DSDF
{
	// https://postd.cc/voronoi-diagrams/
	struct D2JFACell
	{
		i16vec2 nearest_inner;
	};

	enum
	{
		SDF_USE_NUM = 3,
	};
	GI2DSDF(const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_gi2d_context = gi2d_context;
		const auto& context = gi2d_context->m_context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
 			b_jfa = context->m_storage_memory.allocateMemory<D2JFACell>({ gi2d_context->FragmentBufferSize * SDF_USE_NUM,{} });
 			b_sdf = context->m_storage_memory.allocateMemory<vec2>({ gi2d_context->FragmentBufferSize,{} });
		}

		{
			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
 					b_jfa.getInfo(),
 					b_sdf.getInfo(),
				};

				vk::WriteDescriptorSet write[] = {
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}
	}
	void execute(vk::CommandBuffer cmd)
	{
	}
	std::shared_ptr<GI2DContext> m_gi2d_context;

 	btr::BufferMemoryEx<D2JFACell> b_jfa;
 	btr::BufferMemoryEx<vec2> b_sdf;

	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
};