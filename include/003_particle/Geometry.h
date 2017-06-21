#pragma once
#include <vector>
#include <tuple>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/Loader.h>

struct Geometry
{
	struct Resource
	{
		btr::AllocatedMemory m_vertex;
		btr::AllocatedMemory m_index;
		btr::AllocatedMemory m_indirect;
		vk::IndexType m_index_type;
		std::vector<vk::VertexInputAttributeDescription> m_vertex_attribute;
		std::vector<vk::VertexInputBindingDescription> m_vertex_binding;
	};
	std::unique_ptr<Resource> m_resource;
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MakeBox(float edge = 1.f);
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MakePlane();
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> MakeSphere(uint32_t quarity = 1);
	static std::vector<glm::vec3> CalcNormal(const std::vector<glm::vec3>& vertex, const std::vector<glm::uvec3>& element);
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>> createOrthoNormalBasis(const std::vector<glm::vec3>& normal);

	struct OptimaizeDuplicateVertexDescriptor {
		glm::u64vec3 m_mask_size;
		float m_duplicate_distance;
		OptimaizeDuplicateVertexDescriptor()
			: m_mask_size(24ull, 16ull, 24ull)
			, m_duplicate_distance(1000.f)
		{}
	};
	static void OptimaizeDuplicateVertex(std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>>& vertex, const OptimaizeDuplicateVertexDescriptor& desc);
	static void OptimaizeConnectTriangle(std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>>& vertex);

	static Geometry MakeGeometry(
		btr::Loader& loader,
		const void* vertex,
		size_t vertex_size,
		const void* index,
		size_t index_size,
		vk::IndexType index_type,
		const std::vector<vk::VertexInputAttributeDescription>& vertex_attr,
		const std::vector<vk::VertexInputBindingDescription>& vertex_bind
	);


};

