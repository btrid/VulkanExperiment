#pragma once
#include <array>
#include <memory>
#include <mutex>
#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/BufferMemory.h>
namespace btr
{

struct StagingBuffer
{
	AllocatedMemory m_staging_memory;
	void setup(btr::BufferMemory staging_memory, vk::DeviceSize size)
	{
		m_staging_memory = staging_memory.allocateMemory(size);
	}

	template<typename T>
	T* getPtr(vk::DeviceSize offset) { return reinterpret_cast<T*>(m_staging_memory.getMappedPtr()) + offset; }
	vk::DescriptorBufferInfo getBufferInfo()const { return m_staging_memory.getBufferInfo(); }
	vk::DeviceMemory getMemory()const { return m_staging_memory.getDeviceMemory(); }
};
struct LightParam {
	glm::vec4 m_position;
	glm::vec4 m_emission;
};

struct FrustomPoint
{
	glm::vec4 ltn;	//!< nearTopLeft
	glm::vec4 rtn;
	glm::vec4 lbn;
	glm::vec4 rbn;
	glm::vec4 ltf;	//!< nearTopLeft
	glm::vec4 rtf;
	glm::vec4 lbf;
	glm::vec4 rbf;
};

struct ComputePipeline
{
	vk::Pipeline					m_pipeline;
	vk::PipelineLayout				m_pipeline_layout;
	vk::DescriptorSetLayout			m_descriptor_set_layout;

	vk::PipelineShaderStageCreateInfo	m_shader_info;

};
struct Light
{
	struct Frustom2
	{
		Plane p[4];
	};

	virtual bool update()
	{
		return true;
	}

	virtual LightParam getParam()const 
	{
		return LightParam();
	}

	struct FowardPlusPipeline
	{
		struct LightInfo
		{
			glm::uvec2 m_resolution;
			glm::uvec2 m_tile_size;

			glm::uvec2 m_tile_num;
			uint32_t m_active_light_num;
			uint32_t _p;
		};
		enum
		{
			COMPUTE_MAKE_FRUSTOM,
//			COMPUTE_MAKE_LIGHT,
			COMPUTE_CULL_LIGHT,
			COMPUTE_NUM,
		};

		vk::DescriptorBufferInfo getLightBufferInfo()const { return m_private->m_light.getBufferInfo(); }
		vk::DescriptorBufferInfo getLightLLHeadBufferInfo()const { return m_private->m_lightLL_head.getBufferInfo(); }
		vk::DescriptorBufferInfo getLightLLNextBufferInfo()const { return m_private->m_lightLL_next.getBufferInfo(); }

		struct Private
		{
			cDevice m_device;
			LightInfo m_light_info;
			UpdateBuffer<LightInfo> m_light_info_gpu;
			UpdateBuffer<FrustomPoint> m_frustom_point;

			btr::BufferMemory m_uniform_memory;
			btr::BufferMemory m_storage_memory;
			AllocatedMemory m_light;
			AllocatedMemory m_lightLL_head;
			AllocatedMemory m_lightLL_next;
			AllocatedMemory m_frustom;
			AllocatedMemory m_light_counter;

			btr::BufferMemory m_staging_memory;
			btr::StagingBuffer m_light_cpu;

			uint32_t m_light_num;

			std::vector<std::unique_ptr<Light>> m_light_list;
			std::vector<std::unique_ptr<Light>> m_light_list_new;
			std::mutex m_light_new_mutex;

			vk::DescriptorPool m_descriptor_pool;
			vk::PipelineCache m_cache;
			std::array<vk::Pipeline, COMPUTE_NUM> m_pipeline;
			std::array<vk::PipelineLayout, COMPUTE_NUM> m_pipeline_layout;
			std::array<vk::DescriptorSetLayout, COMPUTE_NUM> m_descriptor_set_layout;
			std::vector<vk::DescriptorSet> m_compute_descriptor_set;

			std::array<vk::PipelineShaderStageCreateInfo, COMPUTE_NUM> m_shader_info;
			std::array<vk::PipelineShaderStageCreateInfo, 2> m_shader_info_debug;

			std::array<ComputePipeline, COMPUTE_NUM> m_pipeline_ex;

			void setup(const cDevice& device, btr::BufferMemory& storage_memory, size_t light_num)
			{
				m_device = device;
				m_light_num = light_num;

				glm::uvec2 resolution_size(640, 480);
				glm::uvec2 tile_size(32, 32);
				glm::uvec2 tile_num(resolution_size / tile_size);
				m_light_info.m_resolution = resolution_size;
				m_light_info.m_tile_size = tile_size;
				m_light_info.m_tile_num = tile_num;

				m_storage_memory = storage_memory;
				if (!m_storage_memory.isValid())
				{
					vk::DeviceSize size = sizeof(LightParam) * light_num;
					size += sizeof(uint32_t) * light_num;
					size += sizeof(uint32_t) * light_num * 10;
					size += tile_num.x * tile_num.y * sizeof(Frustom2);
					size += sizeof(uint32_t) * 4;
					m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, size);
				}

				m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, 1024);


				// メモリの確保とか
				{
					m_light = storage_memory.allocateMemory(sizeof(LightParam) * light_num);
					m_lightLL_head = storage_memory.allocateMemory(sizeof(uint32_t) * light_num);
					m_lightLL_next = storage_memory.allocateMemory(sizeof(uint32_t) * light_num * 10);
					m_light_counter = storage_memory.allocateMemory(sizeof(uint32_t));
					m_frustom = storage_memory.allocateMemory(tile_num.x * tile_num.y * sizeof(Frustom2));

					vk::DeviceSize alloc_size = m_light.getBufferInfo().range*sGlobal::FRAME_MAX;
					alloc_size += m_lightLL_head.getBufferInfo().range;
					alloc_size += m_frustom.getBufferInfo().range * sGlobal::FRAME_MAX;
					m_staging_memory.setup(m_device, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible /*| vk::MemoryPropertyFlagBits::eHostCoherent*/, alloc_size);


				}

				m_light_cpu.setup(m_staging_memory, m_light.getBufferInfo().range*sGlobal::FRAME_MAX);
				m_light_info_gpu.setup(m_uniform_memory, m_staging_memory);
				m_frustom_point.setup(m_uniform_memory, m_staging_memory);

				// setup shader
				{
					const char* name[] = {
						"MakeFrustom.comp.spv",
//						"MakeLight.comp.spv",
						"CullLight.comp.spv",
					};
					static_assert(array_length(name) == COMPUTE_NUM, "not equal shader num");

					std::string path = btr::getResourceLibPath() + "shader\\binary\\";
					for (size_t i = 0; i < COMPUTE_NUM; i++) {
						m_shader_info[i].setModule(loadShader(m_device.getHandle(), path + name[i]));
						m_shader_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
						m_shader_info[i].setPName("main");
					}
				}

				{
					// Create compute pipeline
					std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings;
					// MakeFrustom
					std::vector<vk::DescriptorSetLayoutBinding> binding =
					{
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(1)
						.setBinding(0),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(1)
						.setBinding(1),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(8),
					};
					bindings.push_back(binding);
					binding =
					{
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(1)
						.setBinding(0),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(8),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(9),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(10),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(11),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(12),
					};
					bindings.push_back(binding);

					for (size_t i = 0; i < bindings.size(); i++)
					{
						vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
							.setBindingCount(bindings[i].size())
							.setPBindings(bindings[i].data());
						auto descriptor_set_layout = device->createDescriptorSetLayout(descriptor_layout_info);

						vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
							.setSetLayoutCount(1)
							.setPSetLayouts(&descriptor_set_layout);
						m_pipeline_layout[i] = device->createPipelineLayout(pipelineLayoutInfo);
						m_descriptor_set_layout[i] = descriptor_set_layout;
					}

					// DescriptorPool
					{
						std::vector<vk::DescriptorPoolSize> descriptor_pool_size;
						for (auto& binding : bindings)
						{
							for (auto& buffer : binding)
							{
								descriptor_pool_size.emplace_back(buffer.descriptorType, buffer.descriptorCount);
							}
						}
						vk::DescriptorPoolCreateInfo descriptor_pool_info;
						descriptor_pool_info.maxSets = bindings.size();
						descriptor_pool_info.poolSizeCount = descriptor_pool_size.size();
						descriptor_pool_info.pPoolSizes = descriptor_pool_size.data();

						m_descriptor_pool = device->createDescriptorPool(descriptor_pool_info);
					}


					// pipeline cache
					{
						vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
						m_cache = device->createPipelineCache(cacheInfo);
					}
					// Create pipeline
					std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
						vk::ComputePipelineCreateInfo()
						.setStage(m_shader_info[0])
						.setLayout(m_pipeline_layout[0]),
						vk::ComputePipelineCreateInfo()
						.setStage(m_shader_info[1])
						.setLayout(m_pipeline_layout[1]),
					};

					auto p = device->createComputePipelines(m_cache, compute_pipeline_info);
					for (size_t i = 0; i < compute_pipeline_info.size(); i++) {
						m_pipeline[i] = p[i];
					}
				}
				vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
				descriptor_set_alloc_info.setDescriptorPool(m_descriptor_pool);
				descriptor_set_alloc_info.setDescriptorSetCount(m_descriptor_set_layout.size());
				descriptor_set_alloc_info.setPSetLayouts(m_descriptor_set_layout.data());
				m_compute_descriptor_set = device->allocateDescriptorSets(descriptor_set_alloc_info);

				// MakeFrustom
				{
					std::vector<vk::DescriptorBufferInfo> uniforms =
					{
						m_light_info_gpu.getBufferInfo(),
						m_frustom_point.getBufferInfo(),
					};
					std::vector<vk::DescriptorBufferInfo> storages =
					{
						m_frustom.getBufferInfo(),
					};

					vk::WriteDescriptorSet desc;
					desc.setDescriptorType(vk::DescriptorType::eUniformBuffer);
					desc.setDescriptorCount(uniforms.size());
					desc.setPBufferInfo(uniforms.data());
					desc.setDstBinding(0);
					desc.setDstSet(m_compute_descriptor_set[0]);
					device->updateDescriptorSets(desc, {});
					desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
					desc.setDescriptorCount(storages.size());
					desc.setPBufferInfo(storages.data());
					desc.setDstBinding(8);
					desc.setDstSet(m_compute_descriptor_set[0]);
					device->updateDescriptorSets(desc, {});
				}
				// LightCulling
				{
					std::vector<vk::DescriptorBufferInfo> uniforms =
					{
						m_light_info_gpu.getBufferInfo(),
					};
					std::vector<vk::DescriptorBufferInfo> storages =
					{
						m_frustom.getBufferInfo(),
						m_light.getBufferInfo(),
						m_lightLL_head.getBufferInfo(),
						m_lightLL_next.getBufferInfo(),
						m_light_counter.getBufferInfo(),
					};

					vk::WriteDescriptorSet desc;
					desc.setDescriptorType(vk::DescriptorType::eUniformBuffer);
					desc.setDescriptorCount(uniforms.size());
					desc.setPBufferInfo(uniforms.data());
					desc.setDstBinding(0);
					desc.setDstSet(m_compute_descriptor_set[1]);
					device->updateDescriptorSets(desc, {});
					desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
					desc.setDescriptorCount(storages.size());
					desc.setPBufferInfo(storages.data());
					desc.setDstBinding(8);
					desc.setDstSet(m_compute_descriptor_set[1]);
					device->updateDescriptorSets(desc, {});

				}

			}
		};
		std::unique_ptr<Private> m_private;

		void setup(const cDevice& device, btr::BufferMemory& storage_memory, size_t light_num)
		{
			m_private = std::make_unique<Private>();
			m_private->setup(device, storage_memory, light_num);
		}

		void add(std::unique_ptr<Light>&& light)
		{
			m_private->m_light_list_new.push_back(std::move(light));
		}

		void execute(vk::CommandBuffer cmd)
		{
			uint32_t zero = 0;
			cmd.updateBuffer<uint32_t>(m_private->m_light_counter.getBufferInfo().buffer, m_private->m_light_counter.getBufferInfo().offset, { zero });
			{
				auto* camera = cCamera::sCamera::Order().getCameraList()[0];
				Frustom frustom;
				frustom.setup(*camera);
				FrustomPoint frustom_point;
				frustom_point.ltn = glm::vec4(frustom.ltn_, 1.f);
				frustom_point.lbn = glm::vec4(frustom.lbn_, 1.f);
				frustom_point.rtn = glm::vec4(frustom.rtn_, 1.f);
				frustom_point.rbn = glm::vec4(frustom.rbn_, 1.f);
				frustom_point.ltf = glm::vec4(frustom.ltf_, 1.f);
				frustom_point.lbf = glm::vec4(frustom.lbf_, 1.f);
				frustom_point.rtf = glm::vec4(frustom.rtf_, 1.f);
				frustom_point.rbf = glm::vec4(frustom.rbf_, 1.f);

				vk::BufferMemoryBarrier to_copy_barrier;
				to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_copy_barrier.buffer = m_private->m_frustom_point.getBufferInfo().buffer;
				to_copy_barrier.setOffset(m_private->m_frustom_point.getBufferInfo().offset);
				to_copy_barrier.setSize(m_private->m_frustom_point.getBufferInfo().range);
				to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

				vk::BufferMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read_barrier.buffer = m_private->m_frustom_point.getBufferInfo().buffer;
				to_shader_read_barrier.setOffset(m_private->m_frustom_point.getBufferInfo().offset);
				to_shader_read_barrier.setSize(m_private->m_frustom_point.getBufferInfo().range);
				to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

				m_private->m_frustom_point.subupdate(frustom_point);
				m_private->m_frustom_point.update(cmd);
			}
			{
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_private->m_pipeline[COMPUTE_MAKE_FRUSTOM]);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_private->m_pipeline_layout[COMPUTE_MAKE_FRUSTOM], 0, m_private->m_compute_descriptor_set[COMPUTE_MAKE_FRUSTOM], {});
				cmd.dispatch(1,1,1);
			}

			// lightの更新
			{
				// 新しいライトの追加
				std::vector<std::unique_ptr<Light>> light;
				{
					std::lock_guard<std::mutex> lock(m_private->m_light_new_mutex);
					light = std::move(m_private->m_light_list_new);
				}
				m_private->m_light_list.insert(m_private->m_light_list.end(), std::make_move_iterator(light.begin()), std::make_move_iterator(light.end()));
			}

			// 更新と寿命の尽きたライトの削除
			m_private->m_light_list.erase(std::remove_if(m_private->m_light_list.begin(), m_private->m_light_list.end(), [](auto& p) { return !p->update(); }), m_private->m_light_list.end());

			{
				// ライトのデータをdeviceにコピー
				size_t frame_offset = sGlobal::Order().getCurrentFrame() * m_private->m_light_num;
//				auto* p = reinterpret_cast<LightParam*>(m_private->m_device->mapMemory(m_private->m_light_cpu.getMemory(), m_private->m_lightLL_head_cpu.getBufferInfo().offset, m_private->m_lightLL_head_cpu.getBufferInfo().range));
				auto* p = m_private->m_light_cpu.getPtr<LightParam>(0);
				uint32_t index = 0;
				for (auto& it : m_private->m_light_list)
				{
					assert(index < m_private->m_light_num);
					p[index] = it->getParam();
					index++;
				}
				{
					vk::BufferMemoryBarrier to_copy_barrier;
					to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
					to_copy_barrier.buffer = m_private->m_light_info_gpu.getBufferInfo().buffer;
					to_copy_barrier.setOffset(m_private->m_light_info_gpu.getBufferInfo().offset);
					to_copy_barrier.setSize(m_private->m_light_info_gpu.getBufferInfo().range);
					to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
					to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

					vk::BufferMemoryBarrier to_shader_read_barrier;
					to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
					to_shader_read_barrier.buffer = m_private->m_light_info_gpu.getBufferInfo().buffer;
					to_shader_read_barrier.setOffset(m_private->m_light_info_gpu.getBufferInfo().offset);
					to_shader_read_barrier.setSize(m_private->m_light_info_gpu.getBufferInfo().range);
					to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

					m_private->m_light_info.m_active_light_num = index;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
					m_private->m_light_info_gpu.subupdate(m_private->m_light_info);
					m_private->m_light_info_gpu.update(cmd);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
				}

				vk::BufferCopy copy_info;
				copy_info.size = index * sizeof(LightParam);
				copy_info.srcOffset = m_private->m_light_cpu.getBufferInfo().range + frame_offset * sizeof(LightParam);
				copy_info.dstOffset = m_private->m_light.getBufferInfo().offset;

				vk::BufferMemoryBarrier to_copy_barrier;
				to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_copy_barrier.buffer = m_private->m_light.getBufferInfo().buffer;
				to_copy_barrier.setOffset(m_private->m_light.getBufferInfo().offset);
				to_copy_barrier.setSize(copy_info.size);
				to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

				vk::BufferMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read_barrier.buffer = m_private->m_light.getBufferInfo().buffer;
				to_shader_read_barrier.setOffset(m_private->m_light.getBufferInfo().offset);
				to_shader_read_barrier.setSize(copy_info.size);
				to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
				cmd.copyBuffer(m_private->m_light_cpu.getBufferInfo().buffer, m_private->m_light.getBufferInfo().buffer, copy_info);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
			}

			{
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_private->m_pipeline[COMPUTE_MAKE_FRUSTOM]);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_private->m_pipeline_layout[COMPUTE_MAKE_FRUSTOM], 0, m_private->m_compute_descriptor_set[COMPUTE_MAKE_FRUSTOM], {});
				cmd.dispatch(1, 1, 1);
			}

		}

		void executeDebug(vk::CommandBuffer cmd)
		{

		}

	};
};

}

