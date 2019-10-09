#include "PhotonMap.h"

//#define _ITERATOR_DEBUG_LEVEL (0)
#include <filesystem>
#include <vector>
#include <queue>
#include <memory>
#include <random>
#include <thread>
#include <future>
#include <bitset>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <ogldef.h>
#include "Renderer.h"
#include "DrawHelper.h"


struct KDTree
{
	struct KDNode {
		std::array<KDNode*, 2> m_child;
		Photon* m_photon;
		int m_split_axis;

		KDNode()
			: m_child()
			, m_photon(nullptr)
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
	PhotonList mPhoton;

	using BBox = std::array<glm::vec3, 2>;

	bool test()
	{
		// 使い方兼test
		std::vector<Photon> p(10000);
		for (auto& it : p)
		{
			it.mPos = glm::linearRand(glm::vec3(-1000.f), glm::vec3(1000.f));
			it.mDir = glm::normalize(glm::linearRand(glm::vec3(-1.f), glm::vec3(1.f)));
			it.mPower = glm::linearRand(glm::vec3(0.f), glm::vec3(1.f));
		}
		Query query;
		query.pos = glm::linearRand(glm::vec3(-800.f), glm::vec3(800.f));
		query.radius = glm::linearRand(0.f, 30.f) + 30.f;
		query.normal = glm::vec3(0.f, 1.f, 0.f);
		query.use_photon_num = p.size()*2;

		KDTree tree;
		ThreadPool threads;
		tree.makeTree(p, threads);
		auto result = tree.locate(query);

		std::vector<Photon> bruteforce;
		bruteforce.reserve(result.m_nearest_photon.size());
		for (auto& it : tree.mPhoton.m_photon)
		{
			if (isIn(&it, query))
			{
				bruteforce.push_back(it);
			}
		}

		bool ok = true;

		// KDTreeの処理でもすべてのフォトンを網羅できてるか
		assert(ok |= (result.m_nearest_photon.size() == bruteforce.size()));

		printf("%s ok\n", __FUNCTION__);
		return ok;
	}

	struct hierarchy
	{
	private:
		int mIndex;
	public:
		hierarchy(int index) : mIndex(index) {}

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

		hierarchy next(int isRight)const { 
			auto i = index() * 2 + 1 + isRight;
			hierarchy next(i);
			return next;
		}
	};

	void makeTree(const std::vector<Photon>& photon, ThreadPool& threadpool)
	{
		assert(threadpool.isMainThread());

		mPhoton.setup(photon);
		m_root_node.mBuffer = std::vector<KDNode>(photon.size());
		m_root_node.mNode = nullptr;

		BBox b = mPhoton.m_bbox;
		hierarchy root(0);
		m_root_node.mNode = makeTree(mPhoton.m_photon.begin(), mPhoton.m_photon.end(), b, threadpool, root);

		threadpool.wait();
	}

	using itr = std::vector<Photon>::iterator;
	KDNode* makeTree(const itr& begin, const itr& end, const BBox& box, ThreadPool& threadpool, const hierarchy& current)
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
			std::sort(begin, end, [=](const Photon& a, const Photon& b) { return a.mPos[axis] < b.mPos[axis]; });

			auto* node = &m_root_node.mBuffer[current.index()];
			node->m_photon = &*(begin + median);
			node->m_split_axis = axis;

			BBox left = box;
			left[1][axis] = node->m_photon->mPos[axis];
			BBox right = box;
			right[0][axis] = node->m_photon->mPos[axis];
			node->m_child[0] = makeTree(begin, begin + median, left, threadpool, current.next(0));
			node->m_child[1] = makeTree(begin + median + 1, end, right, threadpool, current.next(1));
		};

		if (isThread)
		{
			ThreadJob job;
			job.mFinish = maketree;
			threadpool.enque(job);
		}
		else {
			maketree();
		}
		
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

		float dist = query.pos[node->m_split_axis] - node->m_photon->mPos[node->m_split_axis];
		if (dist + query.radius >= 0.f) {
			locate(query, result, node->m_child[1]);
		}
		if (dist - query.radius <= 0.f) {
			locate(query, result, node->m_child[0]);
		}

		if (isIn(node->m_photon, query)) {
			if (result.size() == query.use_photon_num
				&& glm::distance2(result[query.use_photon_num - 1]->mPos, query.pos) > glm::distance2(node->m_photon->mPos, query.pos))
			{
				result[query.use_photon_num - 1] = node->m_photon;
			}
			if (result.size() != result.capacity())
			{
				result.push_back(node->m_photon);
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
namespace{

	glm::vec3 calcDir(const glm::vec3& n, const glm::vec3& t, const glm::vec3& b)
	{
		// 逆関数法で反射方向を決める
		float r1 = glm::linearRand(0.f, 1.f);
		float phi = r1 * 2.f * glm::pi<float>();

		float r2 = glm::linearRand(0.f, 1.f);
		float r2sq = sqrt(r2);

		float tx = r2sq * cos(phi);
		float ty = r2sq * sin(phi);
		float tz = sqrt(1.f - r2);

		vec3 dir = tz * n + tx * t + ty * b;
		dir = glm::normalize(dir);

		return dir;
	}
	PhotonMap::Mesh createMesh(int type)
	{
		std::vector<glm::vec3> v;
		std::vector<unsigned> e;
		switch (type)
		{
		case 1:
			DrawHelper::GetSphere(v, e);
			break;
		case 2:
			DrawHelper::GetPlane(v, e);
			break;
		default:
			{
				DrawHelper::GetBox(v);
				e.resize(v.size());
				for (int i = 0; i < e.size(); i++) {
					e[i] = i;
				}
			}
			break;
		}
		auto n = DrawHelper::CalcNormal(v, e);
		auto tb = DrawHelper::createOrthoNormalBasis(n);

		PhotonMap::Mesh mesh;
		mesh.mIndex.reserve(e.size() / 3);
		for (int i = 0; i < e.size(); i+=3)
		{
			mesh.mIndex.emplace_back(e[i], e[i + 1], e[i + 2]);
		}
		mesh.mVertex.resize(v.size());
		for (unsigned i = 0; i < v.size(); i++)
		{
			mesh.mVertex[i].mPosition = v[i];
			mesh.mVertex[i].mNormal = n[i];
			mesh.mVertex[i].mTangent = std::get<0>(tb)[i];
			mesh.mVertex[i].mBinormal = std::get<1>(tb)[i];
			mesh.mVertex[i].mTexcoord = glm::vec3(0.f);
			mesh.mVertex[i].mMaterialIndex = 0;
		}

		return mesh;
	}
}
void PhotonMap::Scene::setup(const std::vector<glm::vec3>& v, const std::vector<unsigned>& i)
{
	mVertex = v;
	mIndex = i;
}

PhotonMap::Hitpoint PhotonMap::Scene::intersect(const Ray& ray)
{
	Hitpoint hitpoint;
	hitpoint.mRay = ray;
	float closestDistance = FLT_MAX;
	for (auto& model : mModelList.mData)
	{
		for (auto& mesh : model.mMesh)
		{
			auto boundary = mesh.mMeshAABB;
			auto hitAABB = boundary.intersect(ray);
			if (!std::get<0>(hitAABB)) {
//				continue;
			}

			for (int i = 0; i < mesh.mIndex.size(); i++)
			{
				glm::uvec3 index = mesh.mIndex[i];
				Vertex& a = mesh.mVertex[index[0]];
				Vertex& b = mesh.mVertex[index[1]];
				Vertex& c = mesh.mVertex[index[2]];
				glm::vec3 posA = (model.mWorld * glm::vec4(a.mPosition, 1.f)).xyz();
				glm::vec3 posB = (model.mWorld * glm::vec4(b.mPosition, 1.f)).xyz();
				glm::vec3 posC = (model.mWorld * glm::vec4(c.mPosition, 1.f)).xyz();
				Triangle tri = Triangle(posA, posB, posC);
				if (tri.isDegenerate()) {
					// 縮退三角形は無視
					continue;
				}
				Hit hit;
				if (hit = tri.intersect(ray), hit.mIsHit && hit.mDistance < closestDistance)
				{
					closestDistance = hit.mDistance;
					hitpoint.model = &model;
					hitpoint.a = &a;
					hitpoint.b = &b;
					hitpoint.c = &c;
					hitpoint.mIsFront = true;
					hitpoint.mHit = hit;
				}
				else if (!model.mCullingFace && (hit = tri.getBack().intersect(ray), hit.mIsHit) && hit.mDistance < closestDistance)
				{
					// 裏面
					closestDistance = hit.mDistance;
					hitpoint.model = &model;
					hitpoint.a = &a;
					hitpoint.b = &c;
					hitpoint.c = &b;
					hitpoint.mIsFront = false;
					hitpoint.mHit = hit;
				}
			}
		}
	}

	return hitpoint;

}

void PhotonMap::Scene::push(const Model& model)
{
	mModelList.mData.push_back(model);
	auto& m = mModelList.mData.back();

	glm::vec3 min(FLT_MAX);
	glm::vec3 max(-FLT_MAX);

	for (auto& mesh : m.mMesh)
	{
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [&](const Vertex &v){
			auto p = (model.mWorld * glm::vec4(v.mPosition, 1.f)).xyz();
			for (int i = 0; i < 3; i++)
			{
				if (min[i] > p[i]) { min[i] = p[i]; }
				if (max[i] < p[i]) { max[i] = p[i]; }
			}
		});

		mesh.mMeshAABB.max_ = max;
		mesh.mMeshAABB.min_ = min;

		mesh.mMaterial = model.mMaterial[mesh.mVertex[0].mMaterialIndex];
		mesh.mParent = &m;
	}

}

PhotonMap::PhotonMap()
{
	setupScene();

#if _DEBUG
	int width = 64;
	int height = 48;
	int samplingNum = 1;
	int subpixel = 1;
	float photon_num = 10.f;
	bool isMultithread = false;
#else
	int width = 640;
	int height = 480;
	int samplingNum = 1;
	int subpixel = 1;
	float photon_num = 2000.f; // 1m^2ごとに照射されるphoton
	bool isMultithread = true;
#endif

	float aperture = 1.2f;
	float forcus_dist = 30.f;
	glm::vec3 eye(0.f, 10.f, -30.f);
	glm::vec3 target(0.f, 10.f, 0.f);
	vec3 foward = glm::normalize(target - eye);
	vec3 side = glm::normalize(glm::cross(foward, vec3(0., 1., 0.)));
	vec3 up = glm::normalize(glm::cross(side, foward));

	std::vector<vec4> color(width * height);
	Texture render;
	render.create(TextureTarget::TEXTURE_2D);
	render.image2D(0, InternalFormat::RGBA32F, width, height, Format::RGBA, PixelType::FLOAT);


	while (!Renderer::order()->isClose())
	{
		auto b = std::chrono::system_clock::now();

		// photon_mapのクリア
		for (auto& tls : mThreadPool.getThreadLocalList())
		{
			tls.mPhoton.clear();
		}

		// 光線
		auto make_photon_map = [=]()
		{
			std::random_device rd;
			std::mt19937 engine(rd());
			for (const auto& model : mScene.getModelList().mData)
			{
				for (const auto& mesh : model.mMesh)
				{
					for (int i = 0; i < mesh.mIndex.size(); i++)
					{
						Material material = model.mMaterial[mesh.mVertex[mesh.mIndex[i].x].mMaterialIndex];
						if (!material.isEmission())
						{
							continue;
						}
						glm::uvec3 index = mesh.mIndex[i];
						TriangleMesh tri_mesh(mesh.mVertex[index.x].transform(model.mWorld), mesh.mVertex[index.y].transform(model.mWorld), mesh.mVertex[index.z].transform(model.mWorld));

						// フォトン照射
						int devide = isMultithread ? mThreadPool.getThreadNum() : 1;
//						int devide = 1;
						auto surfaceArea = tri_mesh.getArea();
						int ray_num = static_cast<int>(surfaceArea * (photon_num / devide)) ;
						for (int ray_count = 0; ray_count < ray_num; ray_count++){
							// pos
							auto weight = mThreadPool.getThreadLocal().makeRandomWeight();
							auto v = tri_mesh.getPoint(weight);

							// dir
							auto dir = calcDir(v.mNormal, v.mTangent, v.mBinormal);
							Photon photon;
							photon.mPos = v.mPosition;
							photon.mDir = dir;
							photon.mPower = material.mEmissive.xyz() * surfaceArea / (photon_num / devide);
							mapping(photon, 0);
						}

					}
				}

			}
		};
		{
			ThreadJob job;
			job.mFinish = make_photon_map;
			if (isMultithread)
			{
				mThreadPool.enque(job);
				mThreadPool.enque(job);
				mThreadPool.enque(job);
				mThreadPool.enque(job);
				mThreadPool.enque(job);
				mThreadPool.enque(job);
				mThreadPool.enque(job);
			}
			mThreadPool.enque(job);
		}
		mThreadPool.wait();

		// photonmapを合わせる
		int num = 0;
		for (auto& tls : mThreadPool.getThreadLocalList()) {
			num += tls.mPhoton.size();
		}
		std::vector<Photon> photon_list;
		photon_list.reserve(num);
		for (auto& tls : mThreadPool.getThreadLocalList()) {
			photon_list.insert(photon_list.end(), tls.mPhoton.begin(), tls.mPhoton.end());
		}
		printf("photon_num = %d\n", num);

		KDTree tree;
		tree.makeTree(photon_list, mThreadPool);

		// 画面からの光線
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				auto rendering = [=, &color, &tree]()
				{
					vec3 accumDirect(0.f);
					vec3 accumIndirect(0.f);


					for (int sy = 0; sy < subpixel; sy++)
					{
						for (int sx = 0; sx < subpixel; sx++)
						{
							for (int s = 0; s < samplingNum; s++) 
							{
								float rate = (1.0 / subpixel);
								float rx = sx * rate + rate / 2.f;
								float ry = sy * rate + rate / 2.f;
								// deforcus blur
								glm::vec3 forcus =
									eye
									+ foward * forcus_dist
									+ side * ((rx + x) / width - 0.5f) * forcus_dist
									+ up * ((ry + y) / height - 0.5f) * forcus_dist;
								float lens_radius = aperture * 0.5f;
								auto rand = glm::diskRand(lens_radius);
								auto lens = eye + rand.x * side + rand.y * up;
								glm::vec3 dir = glm::normalize(forcus - lens);

								Ray ray(lens, dir);
								auto hit = mScene.intersect(ray);
								if (!hit.isHit())
								{
									continue;
								}
								auto point = hit.getPoint();

								auto irradiance = calcRadiance(hit, tree, 0);
								accumDirect += irradiance;
							}
						}
					}

					if ((y*width + x) % 30000 == 0)
					{
						auto e = std::chrono::system_clock::now();
						auto sec = std::chrono::duration_cast<std::chrono::seconds>(e - b).count();
						auto mili = std::chrono::duration_cast<std::chrono::milliseconds>(e - b).count() % 1000;
						printf("finish [x, y]=[%d, %d] %lld.%03lld\n", x, y, sec, mili);
					}
					auto c = accumDirect + accumIndirect;
					c /= subpixel*subpixel*samplingNum;
					color[y*width + x] = glm::vec4(c, 1.f);
				};

				ThreadJob job;
				job.mFinish = rendering;
				if (isMultithread)
				{
					mThreadPool.enque(job);
				}
				else {
					rendering();
				}
			}	

		}
		mThreadPool.wait();

		// 描画
		Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		render.subImage2D(0, 0, 0, width, height, Format::RGBA, PixelType::FLOAT, color.data());
		DrawHelper::Order()->drawTexture(render.getHandle());

// 		if (Mouse::Order()->isHold(MouseButton::LEFT)) {
// 			forcus_dist += 2.5f;
// 		}
// 		if (Mouse::Order()->isHold(MouseButton::RIGHT)) {
// 			forcus_dist += -2.5f;
//			forcus_dist = glm::max(forcus_dist, 0.01f);
// 		}
		if (Mouse::Order()->isHold(MouseButton::LEFT)) {
			aperture += 0.5f;
		}
		if (Mouse::Order()->isHold(MouseButton::RIGHT)) {
			aperture += -0.5f;
			aperture = glm::max(aperture, 0.01f);
		}

		Renderer::order()->swapBuffer();
		Renderer::order()->loopEvent();

		auto e = std::chrono::system_clock::now();
		printf("%lld.%03lld\n", std::chrono::duration_cast<std::chrono::seconds>(e - b).count(), std::chrono::duration_cast<std::chrono::milliseconds>(e - b).count()%1000);

	}

}

glm::vec3 calcIndirectRadiance(const KDTree &tree, const PhotonMap::Hitpoint &hit)
{
	PhotonMap::Vertex point = hit.getPoint();
	KDTree::Query query;
	query.radius = 0.3f;
	query.use_photon_num = 64;
	query.pos = point.mPosition;
	query.normal = point.mNormal;
	auto nearest_photon = tree.locate(query);

	auto& material = hit.model->mMaterial[hit.getPoint().mMaterialIndex];

	glm::vec3 accumIndirect;
	if (nearest_photon.m_nearest_photon.empty()) {
		return glm::vec3(0.f);
	}
	auto max_dist2 = glm::distance2(query.pos, nearest_photon.m_nearest_photon.back()->mPos);
	auto max_dist = glm::sqrt(max_dist2);
	for (const auto* p : nearest_photon.m_nearest_photon) {
		auto dist = glm::distance(query.pos, p->mPos);
		auto weight = 1.0f - (dist / (1.1f*max_dist));
		auto brdf = material.getBRDFImportanceSampling(p->mDir, point);
		accumIndirect += p->mPower * brdf * weight;
	}
	accumIndirect = accumIndirect / (1.0 - 2.0 / (3.0 * 1.1f));
	return accumIndirect / (glm::pi<float>() * max_dist2);
}

// SchlickによるFresnelの反射係数の近似
float schlick(float cosine, float a, float b)
{
	float r0 = (a - b) / (a + b);
	r0 = r0*r0;
	return r0 + (1.f - r0)*pow((1.f - cosine), 5.f);
}

glm::vec3 refrectMetal(glm::vec3 inDir, glm::vec3 normal, float metalic)
{
	glm::vec3 ret;
	do {
		ret = glm::normalize(glm::reflect(inDir, normal) + glm::sphericalRand(metalic));
	} while (glm::dot(ret, normal) <= 0.01f);
	return ret;
}
glm::vec3 PhotonMap::calcRadiance(Hitpoint hitpoint, const struct KDTree& tree, int depth)
{
	auto& v = hitpoint.getPoint();
	const auto& material = hitpoint.model->mMaterial[v.mMaterialIndex];

	auto brdf = material.getBRDFImportanceSampling(hitpoint.mRay.d_, v);
	switch (material.scattering())
	{
	case Material::Scattering::DIFFUSE:
		break;
	}

	// 直接+間接
	glm::vec3 diffuse;
	if (material.m_diffuse > 0.f)
	{
		diffuse += ::calcIndirectRadiance(tree, hitpoint) * material.m_diffuse;
	}

	// 鏡面反射
	glm::vec3 specular;
	if (material.m_specular > 0.f)
	{
		Ray specularRay;
		specularRay.d_ = refrectMetal(hitpoint.mRay.d_, v.mNormal, material.m_metalic);
		specularRay.p_ = v.mPosition + specularRay.d_*0.01f;
		auto specHit = mScene.intersect(specularRay);
		if (specHit.isHit()) {
			auto irrad = calcRadiance(specHit, tree, depth+1);
			specular += irrad;
		}

		specular *= material.m_specular;
	}

	if (material.m_opacity > 0.f)
	{
		{
			const auto& ray = hitpoint.mRay;
			auto reflect = glm::reflect(ray.d_, v.mNormal);
			bool into = hitpoint.mIsFront; // レイがオブジェクトから出るのか、入るのか

		   // Snellの法則
//			float nc = into ? 1.0f : 1.5f; // 進行中の屈折率
//			float nt = into ? 1.5f : 1.0f; // 媒質の屈折率
			float nc = 1.0f; // 進行中の屈折率
			float nt = 1.5f; // 媒質の屈折率
			float nnt = nc / nt;
			float ddn = glm::dot(ray.d_, v.mNormal);

			float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);
			if (cos2t < 0.0)
			{
				// 全反射なので鏡面反射
				Ray specularRay;
				specularRay.d_ = refrectMetal(hitpoint.mRay.d_, v.mNormal, material.m_metalic);
				specularRay.p_ = v.mPosition + specularRay.d_*0.01f;
				auto specHit = mScene.intersect(specularRay);
				if (specHit.isHit()) {
					auto irrad = calcRadiance(specHit, tree, depth + 1);
					specular += irrad;
				}
			}
			else
			{
				// 屈折していく方向
				glm::vec3 tdir = glm::refract(ray.d_, v.mNormal, nnt);

				// SchlickによるFresnelの反射係数の近似
				float Re = schlick(-ddn, nc, nt);
				float Tr = 1.0 - Re; // 屈折光の運ぶ光の量
				float probability = Re;

				// 屈折と反射のどちらか一方を追跡する。
				// ロシアンルーレットで決定する。
				std::random_device rd;
				std::mt19937 engine(rd());
				if (std::uniform_real_distribution<float>(0.f, 1.f)(engine) < probability)
				{
					// 反射
					Ray specularRay;
					specularRay.d_ = refrectMetal(hitpoint.mRay.d_, v.mNormal, material.m_metalic);
					specularRay.p_ = v.mPosition + specularRay.d_*0.01f;
					auto specHit = mScene.intersect(specularRay);
					if (specHit.isHit()) {
						auto irrad = calcRadiance(specHit, tree, depth + 1);
						specular += irrad * Re;
					}
				}
				else
				{
					// 屈折
					Ray refractionRay;
					refractionRay.d_ = tdir;
					refractionRay.p_ = v.mPosition + tdir*0.01f;
					auto specHit = mScene.intersect(refractionRay);
					if (specHit.isHit()) {
						auto irrad = calcRadiance(specHit, tree, depth + 1);
						specular += irrad * Tr;
					}
				}
			}
		}
	}

	// 未実装
	glm::vec3 caustic(0.f);

	return material.mEmissive.xyz() + specular + diffuse + caustic;
}

void PhotonMap::mapping(const Photon& photon, int depth)
{
	if (glm::dot(photon.mPower, photon.mPower) <= 0.0001f) {
		return;
	}
	Ray ray;
	ray.p_ = photon.mPos + photon.mDir*0.001f;
	ray.d_ = photon.mDir;
	auto hitpoint = mScene.intersect(ray);
	if (!hitpoint.isHit()) {
		return;
	}

	Vertex v = hitpoint.getPoint();
	auto material = hitpoint.model->mMaterial[v.mMaterialIndex];

	// ロシアンルーレット
	std::random_device rd;
	std::mt19937 engine(rd());
	auto probabry = std::uniform_real_distribution<float>(0.f, 1.f)(engine);
	if (probabry < material.m_diffuse)
	{
		// 拡散反射
		auto sampling = material.importanceSampling(ray, v);
		Photon next;
		next.mPos = v.mPosition + sampling.m_reflect*0.01f;
		next.mDir = sampling.m_reflect;
		next.mPower = photon.mPower*sampling.m_brdf;
		mapping(next, depth + 1);

 	}
	else if (probabry < material.m_diffuse + material.m_specular)
	{
		auto reflect = refrectMetal(hitpoint.mRay.d_, v.mNormal, material.m_metalic);
		// 鏡面反射
		Photon next;
		next.mPos = v.mPosition + reflect*0.01f;
		next.mDir = reflect;
		next.mPower = photon.mPower;
		mapping(next, depth + 1);

		// 鏡面ではphotonは保存しない
		return;
	}
	else if (probabry < material.m_diffuse + material.m_specular + material.m_opacity)
	{
		// 吸収
		auto reflect = refrectMetal(hitpoint.mRay.d_, v.mNormal, material.m_metalic);
		bool into = hitpoint.mIsFront; // レイがオブジェクトから出るのか、入るのか

		// Snellの法則
// 		float nc = into ? 1.0f : 1.5f; // 進行中の屈折率
// 		float nt = into ? 1.5f : 1.0f; // 媒質の屈折率
		float nc = 1.0f; // 進行中の屈折率
		float nt = 1.5f; // 媒質の屈折率
		float nnt = nc / nt;
		float ddn = glm::dot(ray.d_, v.mNormal);

		float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);
		if (cos2t < 0.0)
		{
			// 全反射なので鏡面反射
			Photon next;
			next.mPos = v.mPosition + reflect*0.01f;
			next.mDir = reflect;
			next.mPower = photon.mPower;
			mapping(next, depth + 1);

			// 鏡面ではphotonは保存しない
			return;
		}
		// 屈折していく方向
		glm::vec3 tdir = glm::refract(ray.d_, v.mNormal, nnt);

		// SchlickによるFresnelの反射係数の近似
		float Re = schlick(-ddn, nc, nt);
		float Tr = 1.0 - Re; // 屈折光の運ぶ光の量
		float probability = Re;

		// 屈折と反射のどちらか一方を追跡する。
		// ロシアンルーレットで決定する。
		if (std::uniform_real_distribution<float>(0.f, 1.f)(engine) < probability) {
			// 反射
			// Fresnel係数Reを乗算し、ロシアンルーレット確率prob.で割る。
			// 今、prob.=Reなので Re / prob. = 1.0 となる。
			// よって、now_flux = Multiply(now_flux, obj.color) * Re / probability; が以下の式になる。
			// 屈折の場合も同様。
			Photon next;
			next.mPos = v.mPosition + reflect*0.0001f;
			next.mDir = reflect;
			next.mPower = photon.mPower;// * material.mDiffuse.xyz();
			mapping(next, depth + 1);
		}
		else
		{
			// 屈折
			Photon next;
			next.mPos = v.mPosition + tdir*0.0001f;
			next.mDir = tdir;
			next.mPower = photon.mPower;// * material.mDiffuse.xyz();
			mapping(next, depth + 1);
		}

		return;
	}

//	if (depth >= 1)
	{
		// photon保存
		Photon p = photon;
		p.mPos = v.mPosition;
		mThreadPool.getThreadLocal().mPhoton.push_back(p);
	}

}


glm::vec3 PhotonMap::Material::reflect(const Ray& ray, const Vertex& p) const
{
	// 逆関数法で反射方向
	float r1 = glm::linearRand(0.f, 1.f);
	float phi = r1 * 2.f * glm::pi<float>();

	float r2 = glm::linearRand(0.f, 1.f);
	float r2sq = sqrt(r2);

	float tx = r2sq * cos(phi);
	float ty = r2sq * sin(phi);
	float tz = sqrt(1.f - r2);

	vec3 dir = tz * p.mNormal + tx * p.mTangent + ty * p.mBinormal;
	dir = glm::normalize(dir);
	return dir;
}
void PhotonMap::setupScene()
{
	//	using namespace glm;
	// シーンのセットアップ
	if (0)
	{
		unsigned modelFlag = 0
			| aiProcess_JoinIdenticalVertices
			| aiProcess_ImproveCacheLocality
			| aiProcess_LimitBoneWeights
			| aiProcess_RemoveRedundantMaterials
			| aiProcess_SplitLargeMeshes
			| aiProcess_Triangulate
			| aiProcess_SortByPType
			| aiProcess_FindDegenerates
			| aiProcess_CalcTangentSpace
			//				| aiProcess_FlipUVs
			;

		using namespace Assimp;
		Assimp::Importer importer;
		std::string filepath = "../Resource/Tiny/tiny.x";
		importer.ReadFile(filepath, modelFlag);
		const aiScene* scene = importer.GetScene();

		Mesh mesh;
		std::vector<Material> material(scene->mNumMaterials);
		{
			std::string path = std::tr2::sys::path(filepath).remove_filename().string();
			for (size_t i = 0; i < scene->mNumMaterials; i++)
			{
				auto* aiMat = scene->mMaterials[i];
				auto& mat = material[i];
				aiColor4D color;
				aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
				std::memcpy(&mat.mDiffuse, &color, sizeof(color));
				aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color);
				//				std::memcpy(&mat.mAmbient, &color, sizeof(color));
				aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color);
				//				std::memcpy(&mat.mSpecular, &color, sizeof(color));
				aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color);
				std::memcpy(&mat.mEmissive, &color, sizeof(color));
				//				aiMat->Get(AI_MATKEY_SHININESS, mat.mShininess);
				aiString str;
				aiTextureMapMode mapmode[3];
				aiTextureMapping mapping;
				unsigned uvIndex;
				if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					mat.mTextureDiffuse.mData = std::make_shared<std::vector<glm::vec4>>(data.mData);
					mat.mTextureDiffuse.mSize = data.mSize;
				}
				if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					mat.mTextureAmbient.mData = std::make_shared<std::vector<glm::vec4>>(data.mData);
					mat.mTextureAmbient.mSize = data.mSize;
				}
				if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					mat.mTextureSpecular.mData = std::make_shared<std::vector<glm::vec4>>(data.mData);
					mat.mTextureSpecular.mSize = data.mSize;
				}

				if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					mat.mTextureNormal.mData = std::make_shared<std::vector<glm::vec4>>(data.mData);
					mat.mTextureNormal.mSize = data.mSize;
				}

			}

		}

		{

			unsigned numIndex = 0;
			unsigned numVertex = 0;
			std::vector<int> _vertexSize(scene->mNumMeshes);
			std::vector<int> _materialIndex(scene->mNumMeshes);
			for (size_t i = 0; i < scene->mNumMeshes; i++)
			{
				numVertex += scene->mMeshes[i]->mNumVertices;
				numIndex += scene->mMeshes[i]->mNumFaces;
				_vertexSize[i] = scene->mMeshes[i]->mNumVertices;
			}

			std::vector<Vertex> _vertex;
			_vertex.reserve(numVertex);
			std::vector<glm::uvec3> _index;
			_index.reserve(numIndex);


			for (size_t i = 0; i < scene->mNumMeshes; i++)
			{
				aiMesh* mesh = scene->mMeshes[i];
				_materialIndex[i] = mesh->mMaterialIndex;

				// ELEMENT_ARRAY_BUFFER
				// 三角メッシュとして読み込む
				unsigned offset = _vertex.size();
				for (size_t n = 0; n < mesh->mNumFaces; n++) {
					_index.emplace_back(mesh->mFaces[n].mIndices[0] + offset, mesh->mFaces[n].mIndices[1] + offset, mesh->mFaces[n].mIndices[2] + offset);
				}

				// ARRAY_BUFFER
				std::vector<Vertex> vertex(mesh->mNumVertices);
				for (size_t i = 0; i < mesh->mNumVertices; i++)
				{
					vertex[i].mPosition = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
					if (mesh->HasNormals()) {
						vertex[i].mNormal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
					}
					if (mesh->HasTextureCoords(0)) {
						vertex[i].mTexcoord = glm::vec3(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y, mesh->mTextureCoords[0][i].z);
					}
					if (mesh->HasTangentsAndBitangents())
					{
						vertex[i].mTangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
						vertex[i].mBinormal = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
					}

					vertex[i].mMaterialIndex = mesh->mMaterialIndex;
				}

				_vertex.insert(_vertex.end(), vertex.begin(), vertex.end());
			}
			mesh.mVertex = _vertex;
			mesh.mIndex = _index;
		}



		Model model;
		model.mMesh.push_back(mesh);
		model.mMaterial = material;
		model.mWorld = glm::translate(glm::vec3(0.f, -900.f, 0.f));
		mScene.push(model);
	}

	// ライト
	{
		Mesh mesh = createMesh(2);
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mEmissive = glm::vec4(1.0f);
		material[0].mEmissionPower = 1.f;
		material[0].mType = Material::Type::LIGHT;

		model.mWorld *= glm::translate(glm::vec3(0.f, 19.9f, 0.f));
		model.mWorld *= glm::rotate(glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(6.f, 1.f, 6.f));

		model.mCullingFace = true;
		mScene.push(model);

	}

	//	if (0)
	{
		// 中央のボックス
		Mesh mesh = createMesh(0);

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(1.0f, 0.f, 0.f, 1.f);

		model.mWorld = glm::translate(glm::vec3(0.f, 2.f, -2.f));
		//		model.mWorld *= glm::rotate(glm::radians(60.f), glm::vec3(0.f, 1.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(4.f));

//		model.mCullingFace = false;
		model.mUserID = 100;
		mScene.push(model);

	}
	//		if (0)
	{
		Mesh mesh = createMesh(0);
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 1.f, 0.f, 1.f);

		model.mWorld *= glm::translate(glm::vec3(6.f, 3.5f, 5.f));
		model.mWorld *= glm::rotate(glm::radians(60.f), glm::vec3(0.f, 1.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(4.f));
		model.mUserID = 200;

		//			model.mCullingFace = false;
		mScene.push(model);

	}

	if(0)
	{
		// 鏡
		Mesh mesh = createMesh(0);
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 0.f, 0.f, 1.f);
		material[0].m_diffuse = 0.f;
		material[0].m_specular = 1.f;

		model.mWorld *= glm::translate(glm::vec3(-5.f, 6.5f, 5.f));
		model.mWorld *= glm::rotate(glm::radians(-60.f), glm::vec3(0.f, 1.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(5.f));
		model.mUserID = 200;

		//			model.mCullingFace = false;
		mScene.push(model);
	}

	if(0)
	{
		// ガラス
		Mesh mesh = createMesh(0);
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 0.f, 0.f, 1.f);
		material[0].m_diffuse = 0.f;
//		material[0].m_specular = 1.f;
		material[0].m_opacity = 1.f;
		material[0].m_refraction = 1.5f;

		model.mWorld *= glm::translate(glm::vec3(0.f, 5.5f, -8.f));
		model.mWorld *= glm::rotate(glm::radians(-60.f), glm::vec3(0.f, 1.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(6.f));
		model.mUserID = 200;

		model.mCullingFace = false;
		mScene.push(model);
	}

//	if (0)
	{
		// 金属
		Mesh mesh = createMesh(0);
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.3f, 0.3f, 0.8f, 1.f);
		material[0].m_specular = 1.f;
		material[0].m_metalic = 0.2f;

		model.mWorld *= glm::translate(glm::vec3(0.f, 7.5f, 8.f));
		model.mWorld *= glm::rotate(glm::radians(-60.f), glm::vec3(0.f, 1.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(3.f));
		model.mUserID = 200;

		model.mCullingFace = false;
		mScene.push(model);
	}


	// 箱
	//		if (0)
	{
		//		Model model;
		// 		model.mWorld *= glm::scale(glm::vec3(4000.f, 500.f, 4000.f));
		// 		Mesh mesh = createMesh(0);

		Model model;
		std::vector<glm::vec3> v;
		DrawHelper::GetBox(v);
		for (int i = 0; i < v.size() / 3; i++)
		{
			std::swap(v[i * 3 + 1], v[i * 3 + 2]);
		}

		std::vector<unsigned> _i(v.size());
		for (int i = 0; i < _i.size(); i++) {
			_i[i] = i;
		}
		auto n = DrawHelper::CalcNormal(v, _i);
		auto tb = DrawHelper::createOrthoNormalBasis(n);
		std::for_each(v.begin(), v.end(), [](vec3& p) {p.y += 0.5f; p *= 20.f; });

		Mesh mesh;
		mesh.mVertex.resize(v.size() - 6);
		for (unsigned i = 0; i < v.size() - 6; i++)
		{
			mesh.mVertex[i].mPosition = v[i];
			mesh.mVertex[i].mNormal = n[i];
			mesh.mVertex[i].mTangent = std::get<0>(tb)[i];
			mesh.mVertex[i].mBinormal = std::get<1>(tb)[i];
			mesh.mVertex[i].mTexcoord = glm::vec3(0.f);
			mesh.mVertex[i].mMaterialIndex = i / 6;
		}

		std::vector<glm::uvec3> e((v.size() - 6) / 3);
		for (int i = 0; i < e.size() * 3; i++) {
			e[i / 3][i % 3] = _i[i];
		}
		mesh.mIndex = e;


		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(6);
		int index = 0;
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		index = 0;
		material[index++].m_diffuse = 0.6f;
		material[index++].m_diffuse = 0.6f;
		material[index++].m_diffuse = 0.6f;
		material[index++].m_diffuse = 0.6f;
		material[index++].m_diffuse = 0.6f;
		material[index++].m_diffuse = 0.6f;
		// 		// hidari
		// 		material[index++].mDiffuse = glm::vec4(1.f, 0.f, 1.f, 1.f);
		// 		// migi
		// 		material[index++].mDiffuse = glm::vec4(0.f, 1.f, 0.f, 1.f);
		// 
		// 		material[index++].mDiffuse = glm::vec4(1.f, 0.f, 0.f, 1.f);
		// 		// sita
		// 		material[index++].mDiffuse = glm::vec4(1.f, 1.f, 1.f, 1.f);
		// 		// oku
		// 		material[index++].mDiffuse = glm::vec4(0.f, 0.f, 1.f, 1.f);
		// 		material[index++].mDiffuse = glm::vec4(1.f, 1.f, 0.f, 1.f);

//		model.mCullingFace = false;
		model.mUserID = 50;

		mScene.push(model);

	}
}
