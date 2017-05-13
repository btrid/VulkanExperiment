#pragma once
#include <vector>
#include <tuple>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/BufferMemory.h>
struct Loader
{
	cDevice m_device;
	vk::RenderPass m_render_pass;
	btr::BufferMemory m_vertex_memory;
	btr::BufferMemory m_uniform_memory;
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_staging_memory;

	vk::CommandBuffer m_cmd;
};

struct Geometry
{
	struct Resource
	{
		btr::AllocatedMemory m_vertex;
		btr::AllocatedMemory m_index;
		vk::IndexType m_index_type;
		std::vector<vk::VertexInputAttributeDescription> m_vertex_attribute;
		std::vector<vk::VertexInputBindingDescription> m_vertex_binding;

	};
	std::unique_ptr<Resource> m_resource;
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MakeBox();
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MakePlane();
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MakeSphere(uint32_t quarity = 1);
	static std::vector<glm::vec3> CalcNormal(const std::vector<glm::vec3>& vertex, const std::vector<glm::uvec3>& element);
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>> createOrthoNormalBasis(const std::vector<glm::vec3>& normal);

	static Geometry MakeGeometry(
		Loader& loader,
		const void* vertex,
		size_t vertex_size,
		const void* index,
		size_t index_size,
		vk::IndexType index_type,
		const std::vector<vk::VertexInputAttributeDescription>& vertex_attr,
		const std::vector<vk::VertexInputBindingDescription>& vertex_bind
	);
};

