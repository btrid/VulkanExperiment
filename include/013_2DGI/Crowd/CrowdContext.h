#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct CrowdContext
{
	enum {
		Frame = 1,
		Rot_Num = 32,
	};

	struct CrowdInfo
	{
		uint32_t crowd_info_max;
		uint32_t unit_info_max;
		uint32_t crowd_data_max;
		uint32_t unit_data_max;

		uint ray_num_max;
		uint ray_frame_max;
		uint frame_max;
		uint ray_angle_num;


	};

	struct CrowdScene
	{
		int m_frame;
		int m_hierarchy;
		uint m_skip;
		int _p2;
	};

	struct CrowdData
	{
		vec2 target;
		vec2 _p;
	};

	struct UnitInfo
	{
		float linear_speed;
		float angler_speed;
		float _p2;
		float _p3;
	};
	struct UnitData
	{
		vec2 m_pos;
		float m_rot;
		float m_rot_prev;

		float m_move;
		int32_t crowd_type;
		int32_t unit_type;
		float _p;
	};
	struct CrowdRay
	{
		vec2 origin;
		float angle;
		uint march;
	};
	struct CrowdSegment
	{
		uint ray_index;
		uint begin;
		uint march;
		uint radiance;
	};

	struct PathNode
	{
		uint value;
	};

	CrowdContext(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		auto Ray_Frame_Num = gi2d_context->m_desc.Resolution.x * Rot_Num; // ŽÀÛ‚Í‚à‚¤­‚µ­‚È‚¢
		auto Ray_All_Num = Ray_Frame_Num * Frame;
		auto Segment_Num = Ray_Frame_Num * 64; // ‚Æ‚è‚ ‚¦‚¸‚Ì’l
		{
			m_crowd_info.crowd_info_max = 1;
			m_crowd_info.unit_info_max = 16;
			m_crowd_info.crowd_data_max = 16;
			m_crowd_info.unit_data_max = 128;

			m_crowd_info.frame_max = Frame;
			m_crowd_info.ray_num_max = Ray_All_Num;
			m_crowd_info.ray_frame_max = Ray_Frame_Num;
			m_crowd_info.ray_angle_num = Rot_Num;
			m_crowd_scene.m_frame = 0;
			m_crowd_scene.m_skip = 0;
		}
		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			}
		}

		{
			u_crowd_info = m_context->m_uniform_memory.allocateMemory<CrowdInfo>({ 1, {} });
			u_crowd_scene = m_context->m_uniform_memory.allocateMemory<CrowdScene>({ 1, {} });
			u_unit_info = m_context->m_uniform_memory.allocateMemory<UnitInfo>({ m_crowd_info.unit_info_max, {} });
			b_crowd = m_context->m_storage_memory.allocateMemory<CrowdData>({ m_crowd_info.crowd_data_max, {} });
			b_unit = m_context->m_storage_memory.allocateMemory<UnitData>({ m_crowd_info.unit_data_max*2, {} });
			b_unit_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 1, {} });
			b_unit_link_head = m_context->m_storage_memory.allocateMemory<int32_t>({ gi2d_context->m_desc.Resolution.x*gi2d_context->m_desc.Resolution.y, {} });
			b_ray_counter = m_context->m_storage_memory.allocateMemory<ivec4>({ Frame,{} });
			b_segment_counter = m_context->m_storage_memory.allocateMemory<ivec4>({ 1,{} });
			b_ray = m_context->m_storage_memory.allocateMemory<CrowdRay>({ m_crowd_info.ray_num_max,{} });
			b_segment = m_context->m_storage_memory.allocateMemory<CrowdSegment>({ Segment_Num,{} });
			b_node = m_context->m_storage_memory.allocateMemory<PathNode>({ gi2d_context->m_desc.Resolution.x*gi2d_context->m_desc.Resolution.y,{} });

			vk::DescriptorBufferInfo uniforms[] = {
				u_crowd_info.getInfo(),
				u_crowd_scene.getInfo(),
				u_unit_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				b_crowd.getInfo(),
				b_unit.getInfo(),
				b_unit_counter.getInfo(),
				b_unit_link_head.getInfo(),
				b_ray.getInfo(),
				b_ray_counter.getInfo(),
				b_segment.getInfo(),
				b_segment_counter.getInfo(),
				b_node.getInfo(),
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
				.setDstBinding(3)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}


		// initialize
		{
			{
				auto staging = m_context->m_staging_memory.allocateMemory<CrowdInfo>({ 1, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
				*staging.getMappedPtr() = m_crowd_info;

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(u_crowd_info.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, u_crowd_info.getInfo().buffer, copy);

			}

			{
				auto staging = m_context->m_staging_memory.allocateMemory<UnitData>({ m_crowd_info.unit_data_max * 2, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
				for (int32_t i = 0; i < m_crowd_info.unit_data_max*2; i++)
				{
					staging.getMappedPtr(i)->m_pos = abs(glm::ballRand(300.f).xy()) + vec2(100.f);
					staging.getMappedPtr(i)->m_move = 50.f;
					staging.getMappedPtr(i)->m_rot = (std::rand() % 314) * 0.01f;
					staging.getMappedPtr(i)->m_rot_prev = 0.f;
					staging.getMappedPtr(i)->unit_type = 0;
//					staging.getMappedPtr(i)->crowd_type = std::rand() % 2;
					staging.getMappedPtr(i)->crowd_type = 0;
					staging.getMappedPtr(i)->m_pos += staging.getMappedPtr(i)->crowd_type * vec2(600.f);
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(b_unit.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, b_unit.getInfo().buffer, copy);

			}

			{
				auto staging = m_context->m_staging_memory.allocateMemory<UnitInfo>({ m_crowd_info.unit_info_max, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
				for (int32_t i = 0; i < m_crowd_info.unit_info_max; i++)
				{
					auto& info = *staging.getMappedPtr(i);
					info.linear_speed = 50.f;
					info.angler_speed = 50.5f;
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(u_unit_info.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, u_unit_info.getInfo().buffer, copy);

			}

			{
				cmd.fillBuffer(b_unit_link_head.getInfo().buffer, b_unit_link_head.getInfo().offset, b_unit_link_head.getInfo().range, -1);

			}
			vk::BufferMemoryBarrier to_read[] = {
				u_crowd_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				u_unit_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_unit.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}
	}

	void execute(vk::CommandBuffer cmd)
	{
		m_crowd_scene.m_frame = (m_crowd_scene.m_frame+1)% Frame;
		cmd.updateBuffer<CrowdScene>(u_crowd_scene.getInfo().buffer, u_crowd_scene.getInfo().offset, m_crowd_scene);

	}

	std::shared_ptr<btr::Context> m_context;
	CrowdInfo m_crowd_info;
	CrowdScene m_crowd_scene;
 
	btr::BufferMemoryEx<CrowdInfo> u_crowd_info;
	btr::BufferMemoryEx<CrowdScene> u_crowd_scene;
	btr::BufferMemoryEx<UnitInfo> u_unit_info;
	btr::BufferMemoryEx<CrowdData> b_crowd;
	btr::BufferMemoryEx<UnitData> b_unit;
	btr::BufferMemoryEx<uvec4> b_unit_counter;
	btr::BufferMemoryEx<int32_t> b_unit_link_head;

	btr::BufferMemoryEx<CrowdRay> b_ray;
	btr::BufferMemoryEx<ivec4> b_ray_counter;
	btr::BufferMemoryEx<CrowdSegment> b_segment;
	btr::BufferMemoryEx<ivec4> b_segment_counter;
	btr::BufferMemoryEx<PathNode> b_node;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};
