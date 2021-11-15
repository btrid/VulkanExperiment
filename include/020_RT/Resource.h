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

// https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/
#include <020_RT/nvmeshlet_builder.hpp>
#include <020_RT/nvmeshlet_packbasic.hpp>

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
			m_active[i] = (uint32_t)i;
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
}; template<typename T>
struct ResourceManager3
{
	std::unordered_map<std::string, std::weak_ptr<T>> m_resource_list;
	std::mutex m_mutex;

	ResourceManager3()
	{
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
		resource = std::make_shared<T>();
		resource->m_filename = filename;

		m_resource_list[filename] = resource;
		return false;
	}
};


struct Primitive
{
	vk::DrawMeshTasksIndirectCommandNV m_task;
	uint64_t PrimitiveAddress;

	enum ID
	{
		Vertex,
		Index,

		Normal,
		Texcoord,

		Material,
		_unuse,

		MeshletDesc,
		MeshletPack,

		ID_MAX,
	};
	uint64_t v[ID_MAX];

	vec3 m_aabb_min;
	uint _p;
	vec3 m_aabb_max;
	uint _p2;
};

namespace gltf
{

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
	
struct Texture
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

struct Mesh
{
	std::vector<Primitive> m_primitive;
	btr::BufferMemoryEx<Primitive> b_primitive;

	std::vector<NVMeshlet::PackBasicBuilder::MeshletGeometry> m_MeshletGeometry;
	std::vector<btr::BufferMemoryEx<NVMeshlet::MeshletPackBasicDesc>> b_MeshletDesc;
	std::vector<btr::BufferMemoryEx<NVMeshlet::PackBasicType>> b_MeshletPack;

// private:
// 	~Mesh() = delete;
};

struct Light
{
	vec3 pos;
};

struct Camera
{
	vec3 pos;
};
struct gltfResource
{
	std::string m_filename;
	tinygltf::Model gltf_model;

	std::vector<btr::BufferMemory> b_buffer;
	btr::BufferMemoryEx<Material> b_material;
	std::vector<std::shared_ptr<Texture>> t_image;

	std::vector<Mesh> m_mesh;

	std::vector<Light> m_lights;
	std::vector<Camera> m_camera;


	struct BLAS
	{
		vk::UniqueAccelerationStructureKHR m_AS;
		btr::BufferMemory m_AS_buffer;
	};
	BLAS m_BLAS;

};

}

struct Resource
{
	std::array<vk::DescriptorBufferInfo, 10> m_buffer_info;
	std::array<vk::DescriptorImageInfo, 1024> m_image_info;

	ResourceManager3<gltf::gltfResource> m_gltf_manager;
	ResourceManager2<gltf::Texture> m_texture_manager;

	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS_ModelResource;
	vk::UniqueDescriptorUpdateTemplate m_Texture_DUP;

	Resource(btr::Context& ctx)
	{
		{
			std::array<vk::DescriptorPoolSize, 2> pool_size;
			pool_size[0].setType(vk::DescriptorType::eStorageBuffer);
			pool_size[0].setDescriptorCount(array_size(m_buffer_info));
			pool_size[1].setType(vk::DescriptorType::eCombinedImageSampler);
			pool_size[1].setDescriptorCount(array_size(m_image_info));

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount(array_length(pool_size));
			pool_info.setPPoolSizes(pool_size.data());
			pool_info.setMaxSets(1);
			pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
			m_descriptor_pool = ctx.m_device.createDescriptorPoolUnique(pool_info);

		}

		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eTaskNV | vk::ShaderStageFlagBits::eMeshNV;
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eCombinedImageSampler, 1024, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		m_buffer_info = {
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
			ctx.m_vertex_memory.allocateMemory(0).getInfo(),
												//			b_meshlet_desc.getInfo(),
		};
		m_image_info.fill(vk::DescriptorImageInfo{ sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal });

		{
			vk::DescriptorUpdateTemplateEntry dutEntry[2];
			dutEntry[0].setDstBinding(0).setDstArrayElement(0).setDescriptorCount(array_size(m_buffer_info)).setDescriptorType(vk::DescriptorType::eStorageBuffer).setOffset(offsetof(Resource, m_buffer_info)).setStride(sizeof(VkDescriptorBufferInfo));
			dutEntry[1].setDstBinding(10).setDstArrayElement(0).setDescriptorCount(array_size(m_image_info)).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setOffset(offsetof(Resource, m_image_info)).setStride(sizeof(VkDescriptorImageInfo));

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

	std::shared_ptr<gltf::gltfResource> LoadScene(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename);
private:
	std::shared_ptr<gltf::Texture> LoadTexture(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename, const vk::ImageCreateInfo& info, const std::vector<byte>& data);

};
