#include <applib/Geometry.h>
#include <unordered_map>
#include <set>
std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> Geometry::MakeBox(float edge /*= 1.f*/)
{
	std::vector<glm::vec3> v = 
	{
		{-edge,  edge,  edge},
		{ edge,  edge,  edge},
		{-edge, -edge,  edge},
		{ edge, -edge,  edge},
		{-edge,  edge, -edge},
		{ edge,  edge, -edge},
		{-edge, -edge, -edge},
		{ edge, -edge, -edge},
	};

	std::vector<glm::uvec3> i =
	{
		{0, 2, 3},
		{0, 3, 1},
		{0, 1, 5},
		{0, 5, 4},
		{6, 7, 3},
		{6, 3, 2},
		{0, 4, 6},
		{0, 6, 2},
		{3, 7, 5},
		{3, 5, 1},
		{5, 7, 6},
		{5, 6, 4},
	};
	return std::tie(v, i);
}

std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> Geometry::MakePlane(float edge /*= 1.f*/)
{
	std::vector<glm::uvec3> i =
	{
		{0, 1, 2},
		{1, 3, 2},
	};

	std::vector<glm::vec3> v =
	{
		{-edge, 0.f,  -edge},
		{ edge,  0.f, -edge},
		{-edge, 0.f,   edge},
	 	{ edge,  0.f,  edge}, 
	};
	return std::tie(v, i);
}

std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> Geometry::MakeSphere(uint32_t quarity /*= 1*/)
{
	// http://blog.andreaskahler.com/2009/06/creating-icosphere-mesh-in-code.html
	class SphereCreator
	{

		std::vector<glm::vec3> vertex;
		std::unordered_map<std::int64_t, int> middlePointIndexCache;

		// add vertex to mesh, fix position to be on unit sphere, return index
		int addVertex(const glm::vec3& p)
		{
			vertex.emplace_back(glm::normalize(p));
			return vertex.size() - 1;
		}

		// return index of point in the middle of p1 and p2
		int getMiddlePoint(int p1, int p2)
		{
			// first check if we have it already
			bool firstIsSmaller = p1 < p2;
			std::int64_t smallerIndex = firstIsSmaller ? p1 : p2;
			std::int64_t greaterIndex = firstIsSmaller ? p2 : p1;
			std::int64_t key = (smallerIndex << 32) + greaterIndex;

			auto it = middlePointIndexCache.find(key);
			if (it != middlePointIndexCache.end())
			{
				return it->second;
			}

			// not in cache, calculate it
			glm::vec3 point1 = vertex[p1];
			glm::vec3 point2 = vertex[p2];
			glm::vec3 middle = (point1 + point2) / 2.f;

			// add vertex makes sure point is on unit sphere
			int i = addVertex(middle);

			// store it, return index
			middlePointIndexCache[key] = i;
			return i;
		}

	public:
		std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> create(int recursionLevel)
		{
			// create 12 vertices of a icosahedron
			auto t = (1.f + glm::sqrt(5.f)) / 2.f;

			addVertex(glm::vec3(-1, t, 0));
			addVertex(glm::vec3(1, t, 0));
			addVertex(glm::vec3(-1, -t, 0));
			addVertex(glm::vec3(1, -t, 0));

			addVertex(glm::vec3(0, -1, t));
			addVertex(glm::vec3(0, 1, t));
			addVertex(glm::vec3(0, -1, -t));
			addVertex(glm::vec3(0, 1, -t));

			addVertex(glm::vec3(t, 0, -1));
			addVertex(glm::vec3(t, 0, 1));
			addVertex(glm::vec3(-t, 0, -1));
			addVertex(glm::vec3(-t, 0, 1));


			// create 20 triangles of the icosahedron
			std::vector<glm::uvec3> face;

			// 5 faces around point 0
			face.emplace_back(0, 11, 5);
			face.emplace_back(0, 5, 1);
			face.emplace_back(0, 1, 7);
			face.emplace_back(0, 7, 10);
			face.emplace_back(0, 10, 11);

			// 5 adjacent faces 
			face.emplace_back(1, 5, 9);
			face.emplace_back(5, 11, 4);
			face.emplace_back(11, 10, 2);
			face.emplace_back(10, 7, 6);
			face.emplace_back(7, 1, 8);

			// 5 faces around point 3
			face.emplace_back(3, 9, 4);
			face.emplace_back(3, 4, 2);
			face.emplace_back(3, 2, 6);
			face.emplace_back(3, 6, 8);
			face.emplace_back(3, 8, 9);

			// 5 adjacent faces 
			face.emplace_back(4, 9, 5);
			face.emplace_back(2, 4, 11);
			face.emplace_back(6, 2, 10);
			face.emplace_back(8, 6, 7);
			face.emplace_back(9, 8, 1);


			// refine triangles
			for (int i = 0; i < recursionLevel; i++)
			{
				std::vector<glm::uvec3> face2;
				for (auto& f : face)
				{
					// replace triangle by 4 triangles
					int a = getMiddlePoint(f.x, f.y);
					int b = getMiddlePoint(f.y, f.z);
					int c = getMiddlePoint(f.z, f.x);

					face2.emplace_back(f.x, a, c);
					face2.emplace_back(f.y, b, a);
					face2.emplace_back(f.z, c, b);
					face2.emplace_back(a, b, c);
				}
				face = face2;
			}

			return std::make_tuple(vertex, face);
		}
	};

	return SphereCreator().create(quarity);

}

std::vector<glm::vec3> Geometry::CalcNormal(const std::vector<glm::vec3>& vertex, const std::vector<glm::uvec3>& element)
{
	std::vector<glm::vec3> normal(vertex.size());
	for (size_t i = 0; i < element.size(); i++)
	{
		glm::vec3 a = vertex[element[i].x].xyz;
		glm::vec3 b = vertex[element[i].y].xyz;
		glm::vec3 c = vertex[element[i].z].xyz;
		glm::vec3 ab = glm::normalize(b - a);
		glm::vec3 ac = glm::normalize(c - a);
		glm::vec3 cross = glm::cross(ab, ac);
		normal[element[i].x] += cross;
		normal[element[i].y] += cross;
		normal[element[i].z] += cross;
	}

	for (auto& _n : normal)
	{
		_n = glm::normalize(_n);
	}
	return normal;

}

std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>> Geometry::createOrthoNormalBasis(const std::vector<glm::vec3>& normal)
{
	std::vector<glm::vec3> tangent(normal.size());
	std::vector<glm::vec3> binormal(normal.size());
	for (unsigned i = 0; i < normal.size(); i++)
	{
		const auto& n = normal[i];
		if (abs(n.x) > abs(n.y))
			tangent[i] = glm::normalize(glm::cross(glm::vec3(0.f, 1.f, 0.f), n));
		else
			tangent[i] = glm::normalize(glm::cross(glm::vec3(1.f, 0.f, 0.f), n));
		binormal[i] = glm::normalize(glm::cross(n, tangent[i]));
	}

	return std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>>(std::move(tangent), std::move(binormal));
}

Geometry Geometry::MakeGeometry(std::shared_ptr<btr::Context>& loader, const void* vertex, size_t vertex_size, const void* index, size_t index_size, vk::IndexType index_type, const std::vector<vk::VertexInputAttributeDescription>& vertex_attr, const std::vector<vk::VertexInputBindingDescription>& vertex_bind)
{
	auto cmd = loader->m_cmd_pool->allocCmdTempolary(0);
	auto resource = std::make_unique<Resource>();
	resource->m_vertex_binding = vertex_bind;
	resource->m_vertex_attribute = vertex_attr;
	resource->m_index_type = index_type;
	{
		{
			btr::BufferMemoryDescriptor vertex_desc;
			vertex_desc.size = vertex_size;
			resource->m_vertex = loader->m_vertex_memory.allocateMemory(vertex_desc);

			vertex_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(vertex_desc);
			std::memcpy(staging.getMappedPtr(), vertex, vertex_size);

			vk::BufferCopy vertex_copy;
			vertex_copy.setSize(vertex_size);
			vertex_copy.setSrcOffset(staging.getBufferInfo().offset);
			vertex_copy.setDstOffset(resource->m_vertex.getBufferInfo().offset);
			cmd->copyBuffer(staging.getBufferInfo().buffer, resource->m_vertex.getBufferInfo().buffer, vertex_copy);
		}
		{
			btr::BufferMemoryDescriptor index_desc;
			index_desc.size = index_size;
			resource->m_index = loader->m_vertex_memory.allocateMemory(index_desc);

			index_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(index_desc);
			std::memcpy(staging.getMappedPtr(), index, index_size);

			vk::BufferCopy index_copy;
			index_copy.setSize(index_size);
			index_copy.setSrcOffset(staging.getBufferInfo().offset);
			index_copy.setDstOffset(resource->m_index.getBufferInfo().offset);
			cmd->copyBuffer(staging.getBufferInfo().buffer, resource->m_index.getBufferInfo().buffer, index_copy);
		}

		{
			btr::BufferMemoryDescriptor indirect_desc;
			indirect_desc.size = sizeof(vk::DrawIndexedIndirectCommand);
			resource->m_indirect = loader->m_vertex_memory.allocateMemory(indirect_desc);

			indirect_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(indirect_desc);
			auto* ptr = staging.getMappedPtr<vk::DrawIndexedIndirectCommand>();
			ptr->setFirstInstance(0);
			ptr->setFirstIndex(0);
			ptr->setInstanceCount(1);
			ptr->setIndexCount(index_size / (index_type == vk::IndexType::eUint32 ? sizeof(uint32_t) : sizeof(uint16_t)));
			ptr->setVertexOffset(0);

			vk::BufferCopy indirect_copy;
			indirect_copy.setSize(indirect_desc.size);
			indirect_copy.setSrcOffset(staging.getBufferInfo().offset);
			indirect_copy.setDstOffset(resource->m_indirect.getBufferInfo().offset);
			cmd->copyBuffer(staging.getBufferInfo().buffer, resource->m_indirect.getBufferInfo().buffer, indirect_copy);

		}
	}
	Geometry geo;
	geo.m_resource = std::move(resource);
	return std::move(geo);
}

void Geometry::OptimaizeDuplicateVertex(std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>>& _vertex, const OptimaizeDuplicateVertexDescriptor& desc)
{
	// 0.011と0.019は除去できるが、0.019と0.21は除去できないのでよくない関数。
	// マイナスの頂点があるとだめなので、minをとって足す必要がある。
	std::unordered_map<uint64_t, uint32_t> vertex_cache;
	auto tmp = std::get<0>(_vertex);
	auto& vertex = std::get<0>(_vertex);
	auto& index = std::get<1>(_vertex);
	std::vector<uint32_t> delete_vertex_list(vertex.size());

	assert(desc.m_mask_size.x + desc.m_mask_size.y + desc.m_mask_size.z <= 64);
	auto mask = glm::u64vec3((1ull << desc.m_mask_size.x) - 1, (1ull << (desc.m_mask_size.x+ desc.m_mask_size.y)) - 1, std::numeric_limits<uint64_t>::max());
	mask.z -= mask.y;
	mask.y -= mask.x;
	assert((mask.x | mask.y | mask.z) == std::numeric_limits<uint64_t>::max() && (mask.x & mask.y & mask.z) == 0llu);
	// 重複頂点を探し、使われているVertexにマークを付ける
	for (size_t i = 0; i < index.size(); i ++)
	{
		auto& idx = index[i];
		for (int ii = 0; ii < 3; ii++)
		{
			auto cache_vertex = tmp[idx[ii]] * desc.m_duplicate_distance;
			auto cache_index = glm::u64vec3(cache_vertex);

			// ハッシュの値を超えてしまっていないかチェック。maskの範囲を変えるか、倍率を変える
			assert(glm::all(glm::lessThan(cache_index, glm::u64vec3(1ull) << desc.m_mask_size)));

			auto cache_hash = cache_index.x & mask.x | (cache_index.y << desc.m_mask_size.x) & mask.y | (cache_index.z<< (desc.m_mask_size.x+ desc.m_mask_size.y)) & mask.z;
			auto result = vertex_cache.try_emplace(cache_hash, (uint32_t)idx[ii]);

			if (result.second){
				// 挿入成功したので問題なし
				continue;
			}

			if (idx[ii] == result.first->second)
			{
				//同じ頂点なのでOK
				continue;
			}
			// infにしておく
			if (!glm::any(glm::isinf(vertex.data()[idx[ii]])))
			{
				vertex.data()[idx[ii]] = glm::vec3(std::numeric_limits<float>::infinity());
				delete_vertex_list.data()[idx[ii]]++;
			}
			idx[ii] = result.first->second;
		}
	}

	// infは削除
	auto it = std::remove_if(vertex.begin(), vertex.end(), [](glm::vec3& v) { return glm::any(glm::isinf(v)); });
	vertex.erase(it, vertex.end());

	// vertexを抜いた分のindexの整理
	{
		auto offset = 0;
		for (auto& d : delete_vertex_list)
		{
			auto o = d;
			d += offset;
			offset += o;
		}
		for (size_t idx = 0; idx < index.size(); idx++)
		{
			auto& idx_v = index.data()[idx];
			for (int ii = 0; ii < 3; ii++)
			{
				idx_v[ii] -= delete_vertex_list[idx_v[ii]];
			}
		}
	}
}

void Geometry::OptimaizeConnectTriangle(std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>>& vertex)
{

}
