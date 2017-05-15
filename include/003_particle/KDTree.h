#pragma once

#include <array>
#include <vector>

#include <btrlib/Define.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/Shape.h>
struct KDTree
{
	using BBox = std::array<glm::vec3, 2>;

	struct KDNode {
		std::array<KDNode*, 2> m_child;
		glm::vec3* m_pos;
		int m_split_axis;

		KDNode()
			: m_child()
			, m_pos(nullptr)
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
	std::vector<glm::vec3> m_data;
	BBox m_bbox;

	using BBox = std::array<glm::vec3, 2>;

// 	bool test()
// 	{
// 		// 使い方兼test
// 		std::vector<Photon> p(10000);
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
// 		for (auto& it : tree.mPhoton.m_photon)
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

	void makeTree(const std::vector <glm::vec3> & data)
	{
		assert(threadpool.isMainThread());

//		m_data.setup(photon);
		m_root_node.mBuffer = std::vector<KDNode>(photon.size());
		m_root_node.mNode = nullptr;

		BBox b = m_data.m_bbox;
		Hierarchy root(0);
		m_root_node.mNode = makeTree(m_data.m_photon.begin(), m_data.m_photon.end(), b, threadpool, root);

		threadpool.wait();
	}

	using itr = std::vector<Photon>::iterator;
	KDNode* makeTree(const itr& begin, const itr& end, const BBox& box, ThreadPool& threadpool, const Hierarchy& current)
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

		bool isThread = current.treeNum() > threadpool.getThreadNum();
		auto maketree = [=, &threadpool]() {
			// 分割する軸を決める
			glm::vec3 area = box[1] - box[0];
			int axis = 2;
			if (area.x > area.y && area.x > area.z) {
				axis = 0;
			}
			else if (area.y > area.z) {
				axis = 1;
			}
			std::sort(begin, end, [=](const glm::vec3& a, const glm::vec3& b) { return a[axis] < b[axis]; });

			auto* node = &m_root_node.mBuffer[current.index()];
			node->m_pos = &*(begin + median);
			node->m_split_axis = axis;

			BBox left = box;
			left[1][axis] = node->(*m_pos)[axis];
			BBox right = box;
			right[0][axis] = node->(*m_pos)[axis];
			node->m_child[0] = makeTree(begin, begin + median, left, threadpool, current.next(0));
			node->m_child[1] = makeTree(begin + median + 1, end, right, threadpool, current.next(1));
		};

		maketree();

		return &m_root_node.mBuffer[current.index()];

	}

	struct Query
	{
		glm::vec3 pos;
		float radius;
		glm::vec3 normal;
		int use_photon_num;

	};

	struct QueryResult
	{
		std::vector<Photon*> m_nearest_photon;
		float m_distance2;

		QueryResult()
			: m_distance2(FLT_MAX)
		{

		}
	};

	QueryResult locate(const Query& query)const
	{
		QueryResult result;
		auto& photon = result.m_nearest_photon;
		photon.reserve(query.use_photon_num);
		result.m_distance2 = query.radius *query.radius;
		locate(query, photon, m_root_node.mNode);

		if (!photon.empty())
		{
			result.m_distance2 = glm::distance2(photon[0]->mPos, query.pos);
		}
		return result;
	}

	void locate(const Query& query, std::vector<Photon*>& result, const KDNode* node)const
	{
		if (!node /*|| !node->m_photon*/) {
			return;
		}

		float dist = query.pos[node->m_split_axis] - node->m_pos->mPos[node->m_split_axis];
		if (dist + query.radius >= 0.f) {
			locate(query, result, node->m_child[1]);
		}
		if (dist - query.radius <= 0.f) {
			locate(query, result, node->m_child[0]);
		}

		if (isIn(node->m_pos, query)) {
			if (result.size() == query.use_photon_num
				&& glm::distance2(result[query.use_photon_num - 1]->mPos, query.pos) > glm::distance2(node->m_pos->mPos, query.pos))
			{
				result[query.use_photon_num - 1] = node->m_pos;
			}
			if (result.size() != result.capacity())
			{
				result.push_back(node->m_pos);
			}

			// 距離順にソート
			auto predicate = [=](const auto* a, const auto* b) {
				return glm::distance2(a->mPos, query.pos) < glm::distance2(b->mPos, query.pos);
			};
			std::sort(result.begin(), result.end(), predicate);
		}
	}

private:
	bool isIn(const Photon* p, const Query &query)const
	{
		return glm::distance2(p->mPos, query.pos) <= query.radius*query.radius;
	}

};
