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

template<typename T>
struct ResourceManager4
{

	struct Item : T
	{
		ResourceManager4<T>& m_manager;
		Item(ResourceManager4<T>& manager)
			: m_manager(m_manager)
		{

		}
		~Item()
		{
			m_manager.release(this);
		}
	};
	std::unordered_map<std::string, std::weak_ptr<T>> m_resource_list;
	std::vector<uint32_t> m_active;
	uint32_t m_consume;
	uint32_t m_accume;
	std::mutex m_mutex;

	ResourceManager4()
	{
		m_consume = 0;
		m_accume = 0;
		m_active.resize(1024);
		for (size_t i = 0; i < m_active.size(); i++)
		{
			m_active[i] = (uint32_t)i;
		}
	}

	bool getOrCreate(std::shared_ptr<T>& resource, const std::string& filename)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_resource_list.find(filename);
		if (it != m_resource_list.end()) {
			resource = it->second.lock();
			return true;
		}

		resource = std::make_shared<Item>(*this);
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

struct Primitive
{
	vk::DrawMeshTasksIndirectCommandNV m_task;
	uint64_t PrimitiveAddress;

	uint64_t VertexAddress;
	uint64_t IndexAddress;

	uint64_t NormalAddress;
	uint64_t TexcoordAddress;

	uint64_t TangentAddress;
	uint64_t MaterialAddress;

	uint64_t MeshletDesc;
	uint64_t MeshletPack;

	vec4 m_aabb_min;
//	uint _p;
	vec4 m_aabb_max;
//	bool m_is_emissive = false;
};

namespace gltf
{

struct Material
{
	vec4 m_basecolor_factor;

	float m_metallic_factor;
	float m_roughness_factor;
	float _p2;
	float _p3;

	vec4  m_emissive_factor;

	int TexID_Base;
	int TexID_MR;
	int TexID_AO;
	int TexID_Normal;

	int TexID_Emissive;
	int TexID_p1;
	int TexID_p2;
	int TexID_p3;

// 	bool m_is_emissive_material;
// 	float _p32;
// 	float _p33;
// 	float _p34;
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
	vec3 rot;
};
struct gltfResource
{
	std::string m_filename;
	uint32_t m_block;
	tinygltf::Model gltf_model;

	std::vector<btr::BufferMemory> b_buffer;
	std::vector<Material> m_material;
	btr::BufferMemoryEx<Material> b_material;
	std::vector<std::shared_ptr<Texture>> t_image;

	std::vector<Mesh> m_mesh;
	btr::BufferMemoryEx<uint64_t> b_mesh;
// 	std::vector<Light> m_lights;
// 	std::vector<Camera> m_camera;
// 	
// 	Camera m_default_camera;


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
//#define USE_TEMPLATE
	std::array<vk::DescriptorBufferInfo, 11> m_buffer_info;
	std::array<vk::DescriptorImageInfo, 1024> m_image_info;

	ResourceManager4<gltf::gltfResource> m_gltf_manager;
	ResourceManager2<gltf::Texture> m_texture_manager;

	btr::BufferMemoryEx<uint64_t> b_models;
	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS_ModelResource;
	vk::UniqueDescriptorUpdateTemplate m_Texture_DUP;

	Resource(btr::Context& ctx);

	std::shared_ptr<gltf::gltfResource> LoadScene(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename);
private:
	std::shared_ptr<gltf::Texture> LoadTexture(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename, const vk::ImageCreateInfo& info, const std::vector<byte>& data);

};
