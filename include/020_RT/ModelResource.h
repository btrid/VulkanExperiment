#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <future>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/sGlobal.h>
#include <applib/GraphicsResource.h>

#include <tinygltf/tiny_gltf.h>

template<typename T>
struct ResourceManager2
{
	std::unordered_map<std::string, std::weak_ptr<T>> m_resource_list;
	std::vector<uint32_t> m_active;
	uint32_t m_consume;
	uint32_t m_accume;
	std::mutex m_mutex;

	ResourceManager2()
	{
		m_consume = 0;
		m_accume = 0;
		m_active.resize(1024);
		for (size_t i = 0; i < m_active.size(); i++)
		{
			m_active[i] = i;
		}
	}
	/**
		* @brief	リソースの管理を行う
		*			@return true	すでに管理されている
							false	新しいリソースを生成した
		*/
	bool getOrCreate(std::shared_ptr<T>& resource, const std::string& filename)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_resource_list.find(filename);
		if (it != m_resource_list.end()) {
			resource = it->second.lock();
			return true;
		}
		auto deleter = [&](T* ptr) { release(ptr); sDeleter::Order().enque(std::unique_ptr<T>(ptr)); };
		resource = std::shared_ptr<T>(new T, deleter);
		resource->m_filename = filename;
		resource->m_block = m_active[m_consume];

		m_resource_list[filename] = resource;
		m_consume = (m_consume + 1) % m_active.size();
		return false;
	}

private:
	void release(T* p)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_resource_list.erase(p->m_filename);
		m_active[m_accume] = p->m_block;
		m_accume = (m_accume + 1) % m_active.size();
	}
};


struct Entity
{
	enum ID
	{
		Vertex,
		Normal,

		Texcoord,
		Index,

		Material,
		Material_Index,

		ID_MAX,
	};
	uint64_t v[ID_MAX];

	uint32_t primitive_num;
	uint32_t _p;
	uint32_t _p2;
	uint32_t _p3;
};

struct ModelTexture
{
	std::string m_filename;
	uint32_t m_block;

	vk::UniqueImage m_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_memory;
	vk::UniqueSampler m_sampler;

	vk::DescriptorImageInfo info()
	{
		return vk::DescriptorImageInfo().setSampler(m_sampler.get()).setImageView(m_image_view.get()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
	}

};


struct Model
{
	std::string m_filename;
	uint32_t m_block;

	struct BLAS
	{
		vk::UniqueAccelerationStructureKHR m_AS;
		btr::BufferMemory m_AS_buffer;
	};
	struct Info
	{
		vec4 m_aabb_min;
		vec4 m_aabb_max;
		uint m_vertex_num;
		uint m_primitive_num;
	};

	struct Material
	{
		vec4 m_basecolor_factor;

		float m_metallic_factor;
		float m_roughness_factor;
		float _p1;
		float _p2;

		vec3  m_emissive_factor;
		float _p11;

		enum TexID
		{
			TexID_Base,
			TexID_MR,
			TexID_AO,
			TexID_Normal,

			TexID_Emissive,
			TexID_pad,
			TexID_pad2,
			TexID_pad3,

			TexID_MAX,
		};
		int32_t t[TexID_MAX];
	};
	std::vector<btr::BufferMemory> b_vertex;
	btr::BufferMemoryEx<Material> b_material;
	std::vector<std::shared_ptr<ModelTexture>> t_image;

	tinygltf::Model gltf_model;

	Info m_info;
	BLAS m_BLAS;

 	Entity m_entity;

};

struct ModelResource
{

	std::array<vk::DescriptorBufferInfo, 5> m_buffer_info;
	std::array<vk::DescriptorImageInfo, 1024> m_image_info;

	ResourceManager2<Model> m_model_manager;
	ResourceManager2<ModelTexture> m_texture_manager;
	btr::BufferMemoryEx<Entity> b_model_entity;

	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS_ModelResource;
	vk::UniqueDescriptorUpdateTemplate m_Texture_DUP;

	ModelResource(btr::Context& ctx)
	{
		{
			std::array<vk::DescriptorPoolSize, 2> pool_size;
			pool_size[0].setType(vk::DescriptorType::eStorageBuffer);
			pool_size[0].setDescriptorCount(m_buffer_info.size());
			pool_size[1].setType(vk::DescriptorType::eCombinedImageSampler);
			pool_size[1].setDescriptorCount(m_image_info.size());

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount(array_length(pool_size));
			pool_info.setPPoolSizes(pool_size.data());
			pool_info.setMaxSets(1);
			pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
			m_descriptor_pool = ctx.m_device.createDescriptorPoolUnique(pool_info);

		}

		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eMeshNV;
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eCombinedImageSampler, 1024, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		b_model_entity = ctx.m_vertex_memory.allocateMemory<Entity>(1024);
		m_buffer_info = {
			b_model_entity.getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
		};
		m_image_info.fill(vk::DescriptorImageInfo{ sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal });

		{
			vk::DescriptorUpdateTemplateEntry dutEntry[2];
			dutEntry[0].setDstBinding(0).setDstArrayElement(0).setDescriptorCount(array_size(m_buffer_info)).setDescriptorType(vk::DescriptorType::eStorageBuffer).setOffset(offsetof(ModelResource, m_buffer_info)).setStride(sizeof(VkDescriptorBufferInfo));
			dutEntry[1].setDstBinding(10).setDstArrayElement(0).setDescriptorCount(array_size(m_image_info)).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setOffset(offsetof(ModelResource, m_image_info)).setStride(sizeof(VkDescriptorImageInfo));

			vk::DescriptorUpdateTemplateCreateInfo dutCI;
			dutCI.setTemplateType(vk::DescriptorUpdateTemplateType::eDescriptorSet);
			dutCI.descriptorSetLayout = m_DSL.get();
			dutCI.descriptorUpdateEntryCount = array_size(dutEntry);
			dutCI.pDescriptorUpdateEntries = dutEntry;
			m_Texture_DUP = ctx.m_device.createDescriptorUpdateTemplateUnique(dutCI);
		}
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS_ModelResource = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			ctx.m_device.updateDescriptorSetWithTemplate(*m_DS_ModelResource, *m_Texture_DUP, this);
		}
	}

	std::shared_ptr<Model> LoadModel(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename);
	std::shared_ptr<ModelTexture> LoadTexture(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename, const vk::ImageCreateInfo& info, const std::vector<byte>& data);

};
