#pragma once
#include <array>
#include <vector>

#include <btrlib/Define.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/Shape.h>
struct KDTreeTriangle
{
	using BBox = std::array<glm::vec3, 2>;
	struct MyStruct
	{
		std::vector<glm::vec3>* m_vertex;
		glm::vec3 operator()(const uint32_t& i)const { return (*m_vertex)[i]; }
	};
	using DataAccessor = MyStruct;

	struct TriangleMesh
	{
		std::array<glm::vec3, 3> v;
		glm::uvec3 i;
		glm::vec3 m_normal;
		glm::vec3 center;
		BBox m_bbox;

		glm::vec3 getBBMin()const { return m_bbox[0]; }
		glm::vec3 getBBMax()const { return m_bbox[1]; }
	};
	using itr = std::vector<TriangleMesh>::iterator;

	struct KDNode {
		std::array<KDNode*, 2> m_child;
		TriangleMesh* m_triangle;
		int m_split_axis;

		KDNode()
			: m_child()
			, m_triangle(nullptr)
			, m_split_axis(-1)
		{}

	};

	struct KDNodeRoot
	{
		KDNode* mNode;
		std::vector<KDNode> mBuffer;

		KDNodeRoot()
			: mNode(nullptr)
		{}
		// 		~KDNodeRoot()
		// 		{
		// 			mNode.reset();
		// 		}
	};

	KDNodeRoot m_root_node;
//	DataAccessor m_accessor;
//	std::vector<glm::vec3>* m_vertex_list;
//	std::vector<glm::uvec3>* m_index_list;
	std::vector<TriangleMesh> m_triangle;
	BBox m_bbox;

	struct Hierarchy
	{
	private:
		int mIndex;
	public:
		Hierarchy(int index) : mIndex(index) {}

		bool isRoot()const {
			return mIndex == 0;
		}
		int depth()const
		{
			int c = 0;
			int d = 0;
			for (int i = 1; c < mIndex; i++)
			{
				c += i * 2;
				d++;
			}
			return d;
		}
		int index()const {
			return mIndex;

		}

		int treeNum()const
		{
			int d = depth();
			int n = 1;
			for (int i = 0; i < d; i++)
			{
				n *= 2;
			}
			return n;
		}
		int treeMin()const {
			if (isRoot())
			{
				return 0;
			}
			int d = depth() - 1;
			int n = 1;
			for (int i = 0; i < d; i++)
			{
				n *= 2;
			}
			return n + 1;
		}
		int treeMax()const {
			return treeMin() + treeNum();
		}

		int offset()const
		{
			return mIndex - treeMin();
		}

		Hierarchy next(int isRight)const {
			auto i = index() * 2 + 1 + isRight;
			Hierarchy next(i);
			return next;
		}
	};

	void makeTree(std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>>& geometry)
	{

		auto& vertex_list = std::get<0>(geometry);
		auto& index_list = std::get<1>(geometry);
		m_triangle.resize(index_list.size());
		for (size_t i = 0; i < index_list.size(); i++)
		{
			auto& t = m_triangle[i];
			auto& idx = index_list[i];
			t.v[0] = vertex_list[idx.x];
			t.v[1] = vertex_list[idx.y];
			t.v[2] = vertex_list[idx.z];
			t.i = idx;
			t.center = (t.v[0] + t.v[1] + t.v[2]) / 3.f;
			for (int ii = 0; ii < 3; ii++)
			{
				t.m_bbox[0][0] = glm::min(t.v[ii][0], m_bbox[0][0]);
				t.m_bbox[0][1] = glm::min(t.v[ii][1], m_bbox[0][1]);
				t.m_bbox[0][2] = glm::min(t.v[ii][2], m_bbox[0][2]);
				t.m_bbox[1][0] = glm::max(t.v[ii][0], m_bbox[1][0]);
				t.m_bbox[1][1] = glm::max(t.v[ii][1], m_bbox[1][1]);
				t.m_bbox[1][2] = glm::max(t.v[ii][2], m_bbox[1][2]);
			}
			t.m_normal = glm::normalize(glm::cross(glm::normalize(t.v[1] - t.v[0]), glm::normalize(t.v[2] - t.v[0])));
		}
		m_root_node.mBuffer.resize(index_list.size());
		m_root_node.mNode = nullptr;

		m_bbox[0] = glm::vec3(FLT_MAX);
		m_bbox[1] = glm::vec3(-FLT_MAX);
		for (auto& p : vertex_list)
		{
			for (int i = 0; i < 3; i++)
			{
				m_bbox[0][i] = glm::min(p[i], m_bbox[0][i]);
				m_bbox[1][i] = glm::max(p[i], m_bbox[1][i]);
			}
		}

		BBox b = m_bbox;
		Hierarchy root(0);
		m_root_node.mNode = makeTree(m_triangle.begin(), m_triangle.end(), b, root);
	}

	KDNode* makeTree(const itr& begin, const itr& end, const BBox& box, const Hierarchy& current)
	{
		auto dist = (end - begin);
		auto median = (end - begin) / 2;
		if (dist <= 0)
		{
			return nullptr;
		}

		if (current.index() >= m_root_node.mBuffer.size())
		{
			return nullptr;
		}

		auto maketree = [=]() 
		{
			// 分割する軸を決める
			glm::vec3 area = box[1] - box[0];
			int axis = 2;
			if (area.x > area.y && area.x > area.z) {
				axis = 0;
			}
			else if (area.y > area.z) {
				axis = 1;
			}
			std::sort(begin, end, [=](const TriangleMesh& a, const TriangleMesh& b) { return a.getBBMin()[axis] < b.getBBMin()[axis]; });

			auto* node = &m_root_node.mBuffer[current.index()];
			node->m_triangle = &*(begin + median);
			node->m_split_axis = axis;

			BBox left = box;
			left[1][axis] = node->m_triangle->getBBMin()[axis];
			BBox right = box;
			right[0][axis] = left[1][axis];
			node->m_child[0] = makeTree(begin, begin + median, left, current.next(0));
			node->m_child[1] = makeTree(begin + median + 1, end, right, current.next(1));
		};

		maketree();

		return &m_root_node.mBuffer[current.index()];

	}

	struct Query
	{
		glm::vec3 pos;
		glm::vec3 normal;
		float m_dot;
		float radius;
	};

	struct QueryResult
	{
		std::vector<TriangleMesh*> m_nearest_data;

		QueryResult()
//			: m_distance2(FLT_MAX)
		{
		}
	};

	QueryResult locate(const Query& query)const
	{
		QueryResult result;
		auto& photon = result.m_nearest_data;
		locate(query, photon, m_root_node.mNode);

		return result;
	}

	void locate(const Query& query, std::vector<TriangleMesh*>& result, const KDNode* node)const
	{
		if (!node /*|| !node->m_photon*/) {
			return;
		}

		float dist = query.pos[node->m_split_axis] - node->m_triangle->center[node->m_split_axis];
		if (dist + query.radius >= 0.f) {
			locate(query, result, node->m_child[1]);
		}
		if (dist - query.radius <= 0.f) {
			locate(query, result, node->m_child[0]);
		}

		if (isIn(node->m_triangle, query)) {
// 			if (result.size() == query.use_photon_num
// 				&& glm::distance2(result[query.use_photon_num - 1]->mPos, query.pos) > glm::distance2(node->m_value->mPos, query.pos))
// 			{
// 				result[query.use_photon_num - 1] = node->m_value;
// 			}
// 			if (result.size() != result.capacity())
// 			{
// 				result.push_back(node->m_value);
// 			}
// 
// 			// 距離順にソート
// 			auto predicate = [=](const auto* a, const auto* b) {
// 				return glm::distance2(a->mPos, query.pos) < glm::distance2(b->mPos, query.pos);
// 			};
// 			std::sort(result.begin(), result.end(), predicate);
			result.push_back(node->m_triangle);
		}
	}

private:
	bool isIn(const TriangleMesh* p, const Query &query)const
	{
		return glm::distance2(p->center, query.pos) <= query.radius*query.radius
			&& glm::dot(p->m_normal, query.normal) >= query.m_dot;
	}


};


// 	bool test()
// 	{
// 		// 使い方兼test
// 		std::vector<Value> p(10000);
// 		for (auto& it : p)
// 		{
// 			it.mPos = glm::linearRand(glm::vec3(-1000.f), glm::vec3(1000.f));
// 			it.mDir = glm::normalize(glm::linearRand(glm::vec3(-1.f), glm::vec3(1.f)));
// 			it.mPower = glm::linearRand(glm::vec3(0.f), glm::vec3(1.f));
// 		}
// 		Query query;
// 		query.pos = glm::linearRand(glm::vec3(-800.f), glm::vec3(800.f));
// 		query.radius = glm::linearRand(0.f, 30.f) + 30.f;
// 		query.normal = glm::vec3(0.f, 1.f, 0.f);
// 		query.use_photon_num = p.size() * 2;
// 
// 		KDTree tree;
// 		ThreadPool threads;
// 		tree.makeTree(p, threads);
// 		auto result = tree.locate(query);
// 
// 		std::vector<Photon> bruteforce;
// 		bruteforce.reserve(result.m_nearest_photon.size());
// 		for (auto& it : tree.m_data.m_photon)
// 		{
// 			if (isIn(&it, query))
// 			{
// 				bruteforce.push_back(it);
// 			}
// 		}
// 
// 		bool ok = true;
// 
// 		// KDTreeの処理でもすべてのフォトンを網羅できてるか
// 		assert(ok |= (result.m_nearest_photon.size() == bruteforce.size()));
// 
// 		printf("%s ok\n", __FUNCTION__);
// 		return ok;
// 	}

