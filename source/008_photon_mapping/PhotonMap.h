#pragma once
#include <vector>
#include <list>
#include <memory>
#include <random>
#include "Shape.h"
#include <ProgramPipeline.h>
#include <Renderer.h>
#include "Geometry.h"
#include "ThreadPool.h"

struct Photon
{
	glm::vec3 mPos;
	glm::vec3 mDir;
	glm::vec3 mPower; //!< flux 放射束

	// KDTree用
	char m_split_axis;
	Photon()
		: m_split_axis(-1)
	{}

};

struct PhotonList
{
	std::vector<Photon> m_photon;
	std::array<glm::vec3, 2> m_bbox;

	PhotonList()
	{}

	void setup(const std::vector<Photon>& photon)
	{
		m_photon = photon;

		m_bbox[0] = glm::vec3(FLT_MAX);
		m_bbox[1] = glm::vec3(-FLT_MAX);
		for (auto& p : m_photon)
		{
			for (int i = 0; i < 3; i++)
			{
				m_bbox[0][i] = glm::min(p.mPos[i], m_bbox[0][i]);
				m_bbox[1][i] = glm::max(p.mPos[i], m_bbox[1][i]);
			}
		}
	}

	const glm::vec3& getBoundaryMin()const { return m_bbox[0]; }
	const glm::vec3& getBoundaryMax()const { return m_bbox[1]; }
};

struct NearestPhoton
{
	glm::vec3 mPos;
	glm::vec3 mDir;
	glm::vec3 mPower;
};

struct UserData
{
	std::mt19937 mRandomEngine;
	std::vector<Photon> mPhoton;
	float m_max_irradiance;

	UserData()
	{
		std::random_device rd;
		mRandomEngine.seed(rd());
	}

	glm::vec3 makeRandomWeight()
	{
		std::uniform_real_distribution<float> r01(0.f, 1.f);
		auto weight = glm::vec3(r01(mRandomEngine), r01(mRandomEngine), r01(mRandomEngine));
		weight += glm::vec3(0.f, 0.f, 0.000001f);
		{
			float accum = 0.f;
			for (int i = 0; i < weight.length(); i++)
			{
				accum += weight[i];
			}
			weight /= accum;
		}

		return weight;
	}
};
using ThreadPool = ThreadPool_t<ThreadWorker, ThreadJob, UserData, 8>;
struct PhotonMap
{
	struct Vertex;
	struct Material;
	struct Model;
	struct Hitpoint
	{
		Model* model;
		Vertex* a;
		Vertex* b;
		Vertex* c;
		Hit mHit;
		Ray mRay;
		bool mIsFront;
		Hitpoint()
			: model(nullptr)
			, a(nullptr)
			, b(nullptr)
			, c(nullptr)
			, mIsFront(true)
		{}

		bool isHit()const{ return !!model; }
		glm::vec3 getNormal()const{ 
//			return a ? Triangle(*a, *b, *c).getNormal() : glm::vec3(0.f);
//			return glm::vec3(0.f);
			glm::vec3 pa = (model->mWorld * glm::vec4(a->mPosition, 1.f)).xyz();
			glm::vec3 pb = (model->mWorld * glm::vec4(b->mPosition, 1.f)).xyz();
			glm::vec3 pc = (model->mWorld * glm::vec4(c->mPosition, 1.f)).xyz();
			return Triangle(pa, pb, pc).getNormal();
		}
		Vertex getPoint()const
		{
			auto w = mHit.mWeight;
			Vertex v;
			auto aa = a->transform(model->mWorld);
			auto bb = b->transform(model->mWorld);
			auto cc = c->transform(model->mWorld);
			v.mTexcoord = w.x * a->mTexcoord + w.y * b->mTexcoord + w.z * c->mTexcoord;
			v.mPosition = w.x * aa.mPosition + w.y * bb.mPosition + w.z * cc.mPosition;
//			v.mNormal = w.x * a->mNormal + w.y * b->mNormal + w.z * c->mNormal;
			{
				glm::vec3 pa = (model->mWorld * glm::vec4(a->mPosition, 1.f)).xyz();
				glm::vec3 pb = (model->mWorld * glm::vec4(b->mPosition, 1.f)).xyz();
				glm::vec3 pc = (model->mWorld * glm::vec4(c->mPosition, 1.f)).xyz();
				v.mNormal = Triangle(pa, pb, pc).getNormal();

			}
			{
				auto tb = DrawHelper::createOrthoNormalBasis({v.mNormal});
				v.mTangent = std::get<0>(tb)[0];
				v.mBinormal = std::get<1>(tb)[0];
			}
			v.mMaterialIndex = a->mMaterialIndex;

			return v;
		}

	};
	struct TextureCPU
	{
		std::shared_ptr<std::vector<glm::vec4>> mData;
		glm::ivec3 mSize;

		bool enable()const{
			return mData != NULL;
		}
		glm::vec4 sampling(const glm::ivec2& p)const{
			int index = p.x + p.y*mSize.x;
			return mData->at(index);
		}
		glm::vec4 sampling(const glm::vec2& uv)const{
			glm::ivec2 p = uv * glm::vec2(mSize.xy());
			return sampling(p);
		}
	};
	struct Material
	{
		enum class Type
		{
			NONE,
			LAMBERT,
			PHONG,
			REFLECTION,
			SPECULAR,
			LIGHT,
		};

		int mID;
		Type mType;

		glm::vec4 mDiffuse;
		glm::vec4 mEmissive;

		float mEmissionPower;
		float m_diffuse;
		float m_specular; //!< 光沢
		float m_opacity;	//!< 透過率

		float m_refraction; //!< 屈折率 空気を1.0とする
		float m_rough; //!< ざらざら
		float m_metalic; //!< fuzzy

		TextureCPU mTextureDiffuse;
		TextureCPU mTextureAmbient;
		TextureCPU mTextureSpecular;
		TextureCPU mTextureNormal;
		Material()
			: mID(0)
			, mType(Type::NONE)
			, mDiffuse(0.f)
			, mEmissive(0.f)
			, mEmissionPower(0.f)
			, m_diffuse(0.5f)
			, m_specular(0.f)
			, m_opacity(0.f)
			, m_refraction(1.f)
			, m_rough(0.f)
			, m_metalic(0.f)
		{

		}

		glm::vec3 reflect(const Ray& ray, const Vertex& p)const;

		enum class Scattering : int
		{
			DIFFUSE,
			SPECULAR,
			OPACITY,
			ABSORPTION,
		};
		struct SamplingResult
		{
			// photonmap p181
			// モンテカルロ積分のimportance_samplingのための
			// 確率密度関数 probability_density_function
			float m_pdf;
			// 双方向反射率分布関数 Bidirectional Reflectance Distribution Function
			glm::vec3 m_brdf;

			// 反射方向
			glm::vec3 m_reflect;
		};
		SamplingResult uniformSampling(const Ray& ray, const Vertex& p)const
		{
			return SamplingResult();
		}
		SamplingResult importanceSampling(const Ray& ray, const Vertex& p)const
		{
 			SamplingResult result;
			result.m_reflect = reflect(ray, p);
//			result.m_reflect = glm::normalize(reflect(ray, p) + glm::sphericalRand(m_metalic));
			result.m_pdf = getPDF(result.m_reflect, p.mNormal);
			result.m_brdf = getBRDFImportanceSampling(ray.d_, p);
			return result;
		}
		float getPDF(const glm::vec3& out, const glm::vec3& normal)const 
		{
			float cos_value = glm::dot(out, normal);
			return cos_value / glm::pi<float>();
		}
		glm::vec3 getBRDFImportanceSampling(const glm::vec3& in, const Vertex& p)const
		{
			glm::vec4 diffuse = mTextureDiffuse.enable() ? mTextureDiffuse.sampling(p.mTexcoord.xy()) : mDiffuse;
			return diffuse.xyz() / glm::pi<float>();

		}
		bool isEmission()const { return glm::length2(mEmissive) > 0.f; }

		Scattering scattering()const {
			std::random_device rd;
			std::mt19937 engine(rd());
			auto probabry = std::uniform_real_distribution<float>(0.f, 1.f)(engine);
			if (probabry < m_diffuse)
			{
				// 拡散反射
				return Scattering::DIFFUSE;
			}
			else if (probabry < m_diffuse + m_specular)
			{
				// 鏡面反射
				return Scattering::SPECULAR;
			}
			else if (probabry < m_diffuse + m_specular + m_opacity)
			{
				// 屈折
				return Scattering::OPACITY;
			}
			//	吸収
			return Scattering::ABSORPTION;
		}

	};

	struct Vertex{
		glm::vec3 mPosition;
		glm::vec3 mNormal;
		glm::vec3 mTangent;
		glm::vec3 mBinormal;
		glm::vec3 mTexcoord;
		int mMaterialIndex;

		Vertex transform(const glm::mat4& world)const {
			Vertex v = *this;
			v.mPosition = (world * glm::vec4(v.mPosition, 1.f)).xyz();
			v.mNormal = (world * glm::vec4(v.mNormal, 1.f)).xyz();
			v.mTangent = (world * glm::vec4(v.mTangent, 1.f)).xyz();
			v.mBinormal = (world * glm::vec4(v.mBinormal, 1.f)).xyz();
			return v;		
		}
	};

	struct VertexHit {
		glm::vec3 mPosition;
		glm::vec3 mNormal;
		glm::vec3 mTangent;
		glm::vec3 mBinormal;
		glm::vec3 mTexcoord;
		int mMaterialIndex;
	};


	struct TriangleMesh
	{
		union 
		{
			struct {
				Vertex a;
				Vertex b;
				Vertex c;
			};
			struct 
			{
				Vertex mVertex[3];
			};

		};

		TriangleMesh(const Vertex& _a, const Vertex& _b, const Vertex& _c)
			: a(_a)
			, b(_b)
			, c(_c)
		{}


		Vertex getPoint(const glm::vec3& weight)const
		{
			auto w = weight;
			Vertex v;
			v.mTexcoord = w.x * a.mTexcoord + w.y * b.mTexcoord + w.z * c.mTexcoord;
			v.mPosition = w.x * a.mPosition + w.y * b.mPosition + w.z * c.mPosition;
			{
				glm::vec3 pa = a.mPosition;
				glm::vec3 pb = b.mPosition;
				glm::vec3 pc = c.mPosition;
				v.mNormal = Triangle(pa, pb, pc).getNormal();
			}
			{
				auto tb = DrawHelper::createOrthoNormalBasis({ v.mNormal });
				v.mTangent = std::get<0>(tb)[0];
				v.mBinormal = std::get<1>(tb)[0];
			}
			v.mMaterialIndex = a.mMaterialIndex;

			return v;
		}
		float getArea()const {
			return Triangle(a.mPosition, b.mPosition, c.mPosition).getArea();
		}
		
	};
	struct Mesh
	{
		Model* mParent;
		std::vector<Vertex>		mVertex;
		std::vector<glm::uvec3>	mIndex;

		Material mMaterial;
		AABB mMeshAABB;

		TriangleMesh getTriangle(int i)const
		{
			const glm::uvec3& ii = mIndex[i];
			return TriangleMesh (mVertex[ii.x].transform(mParent->mWorld), mVertex[ii.y].transform(mParent->mWorld), mVertex[ii.z].transform(mParent->mWorld));
		}

	};

	struct Model
	{
		std::vector<Mesh> mMesh;
		std::vector<Material>	mMaterial;

		bool mCullingFace;
		glm::mat4 mWorld;

		int mUserID;	//!< デバッグの時用の識別ID

		Model()
			: mCullingFace(true)
			, mWorld(1.f)
			, mUserID(0)
		{
		}

	};

	struct VolumeModel 
	{
		glm::vec3 scatter(const Ray& in, const Hitpoint& hit, const glm::vec3& power)
		{
			auto v = hit.getPoint();
			auto dist = glm::distance(v.mPosition, in.p_);

			for (int i = 0; i < dist / 2.f + 1; i++)
			{

			}
//			in.p_ + in.d_ *
//			glm::simplex()
		}

		float m_density;
	};
	struct ModelList
	{
//		std::vector<Model> mData;
		std::list<Model> mData;
	};

	struct Scene{
	private:
		ModelList mModelList;
		std::vector<glm::vec3>	mVertex;
		std::vector<unsigned>	mIndex;

		glm::vec4 mBackgroundColor;
	public:
		Scene()
			: mBackgroundColor(0.f, 0.f, 0.f, 1.f)
		{}
		void setup(const std::vector<glm::vec3>& v, const std::vector<unsigned>& i);
		void push(const Model& model);
		glm::vec4 radiance(const Ray& ray, int depth = 0);
		Hitpoint intersect(const Ray& ray);
		const ModelList& getModelList()const { return mModelList; }

	};

	Scene mScene;
	struct UserData
	{
		std::mt19937 mRandomEngine;
		std::vector<Photon> mPhoton;
		float m_max_irradiance;

		UserData()
		{
			std::random_device rd;
			mRandomEngine.seed(rd());
		}

		glm::vec3 makeRandomWeight() 
		{
			std::uniform_real_distribution<float> r01(0.f, 1.f);
			auto weight = glm::vec3(r01(mRandomEngine), r01(mRandomEngine), r01(mRandomEngine));
			weight += glm::vec3(0.f, 0.f, 0.000001f);
			{
				float accum = 0.f;
				for (int i = 0; i < weight.length(); i++)
				{
					accum += weight[i];
				}
				weight /= accum;
			}

			return weight;
		}
	};
	ThreadPool mThreadPool;

	PhotonMap();

	glm::vec3 calcRadiance(PhotonMap::Hitpoint hit, const struct KDTree& tree, int depth);

	void mapping(const Photon& photon, int depth);

	void setupScene();

};

