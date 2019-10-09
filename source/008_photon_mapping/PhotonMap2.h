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

namespace pm2
{
	struct Vertex;
	struct Material;
	struct Model;
/*
std::uint32_t compressDir(const glm::vec3& dir)
{
	float pi = glm::pi<float>();
	float intmax = static_cast<float>(INT16_MAX);
	std::int32_t theta = acos(dir.z) / pi * intmax;
	std::int32_t phi = (dir.y < 0.f ? -1.f : 1.f) * acos(dir.x / sqrt(dir.x*dir.x + dir.y*dir.y)) / pi * intmax;
	return (theta & 0xffff) << 16 | ((phi & 0xffff));

}
glm::vec3 decompressDir(std::uint32_t dir) {
	std::uint16_t theta = dir >> 16;
	std::uint16_t phi = dir & 0xFFFF;

	float pi = glm::pi<float>();
	float intmax = static_cast<float>(INT16_MAX);
	return glm::vec3(
		sin(theta*pi / intmax)*cos(phi*pi / intmax),
		sin(theta*pi / intmax)*sin(phi*pi / intmax),
		cos(theta*pi / intmax));
}
*/
struct Photon
{
	glm::vec3 mPos;
	std::uint32_t dir_16_16; //ã…ç¿ïW[theta, phi]
	glm::vec3 mPower; //!< flux ï˙éÀë©
	std::uint32_t power10_11_11;

	Photon()
	{}

	void setDir(const glm::vec3 dir)
	{
		float pi = glm::pi<float>();
		float intmax = static_cast<float>(INT16_MAX);
		std::int32_t theta = acos(dir.z) / pi * intmax;
		std::int32_t phi = (dir.y < 0.f ? -1.f : 1.f) * acos(dir.x / sqrt(dir.x*dir.x + dir.y*dir.y)) / pi * intmax;
		dir_16_16 = (theta & 0xffff) << 16 | ((phi & 0xffff));

	}
	glm::vec3 getDir()
	{
		std::uint16_t theta = dir_16_16 >> 16;
		std::uint16_t phi = dir_16_16 & 0xFFFF;

		float pi = glm::pi<float>();
		float intmax = static_cast<float>(INT16_MAX);
		return glm::vec3(
			sin(theta*pi / intmax)*cos(phi*pi / intmax),
			sin(theta*pi / intmax)*sin(phi*pi / intmax),
			cos(theta*pi / intmax));
	}
};

struct Ray2 {
	glm::vec3 mPos;
	std::uint32_t mDir_16_16;
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

	glm::vec4 mDiffuse;
	glm::vec4 mEmissive;
	float m_diffuse;
	float m_specular; //!< åıëÚ
	float m_rough; //!< Ç¥ÇÁÇ¥ÇÁ
	float m_metalic;
	Material()
		: mDiffuse(0.f)
		, mEmissive(0.f)
		, m_diffuse(0.5f)
		, m_specular(0.f)
		, m_rough(0.f)
		, m_metalic(0.f)
	{

	}

	struct SamplingResult
	{
		// photonmap p181
		// ÉÇÉìÉeÉJÉãÉçêœï™ÇÃimportance_samplingÇÃÇΩÇﬂÇÃ
		// ämó¶ñßìxä÷êî probability_density_function
		float m_pdf;
		// ëoï˚å¸îΩéÀó¶ï™ïzä÷êî Bidirectional Reflectance Distribution Function
		glm::vec3 m_brdf;

		// îΩéÀï˚å¸
		glm::vec3 m_reflect;
	};
	SamplingResult uniformSampling(const Ray& ray, const Vertex& p)const
	{
		return SamplingResult();
	}
	SamplingResult importanceSampling(const Ray& ray, const Vertex& p)const
	{
		SamplingResult result;
//		result.m_reflect = reflect(ray, p);
//		result.m_pdf = getPDF(result.m_reflect, p.mNormal);
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
//		glm::vec4 diffuse = mTextureDiffuse.enable() ? mTextureDiffuse.sampling(p.mTexcoord.xy()) : mDiffuse;
		auto diffuse = mDiffuse;
		return diffuse.xyz() / glm::pi<float>();

	}
	bool isEmission()const { return glm::length2(mEmissive) > 0.f; }

	int scattering()const {
		std::random_device rd;
		std::mt19937 engine(rd());
		auto probabry = std::uniform_real_distribution<float>(0.f, 1.f)(engine);
		if (probabry < m_diffuse)
		{
			// ägéUîΩéÀ
			return 1;
		}
		else if (probabry < m_diffuse + m_specular)
		{
			// ãæñ îΩéÀ
			return 2;
		}

		//	ãzé˚
		return 0;
	}

};

struct Vertex {
	glm::vec3 mPosition;
	float _p;
 	glm::vec3 mNormal;
	float _n;
	int mMaterialIndex;
	float _m1;
	float _m2;
	float _m3;

	Vertex transform(const glm::mat4& world)const {
		Vertex v = *this;
		v.mPosition = (world * glm::vec4(v.mPosition, 1.f)).xyz();
 		v.mNormal = (world * glm::vec4(v.mNormal, 1.f)).xyz();
// 		v.mTangent = (world * glm::vec4(v.mTangent, 1.f)).xyz();
// 		v.mBinormal = (world * glm::vec4(v.mBinormal, 1.f)).xyz();
		return v;
	}
};
struct VertexGPU {
	glm::vec3 mPosition;
	int mMaterialIndex;

	VertexGPU() = default;
	VertexGPU(const glm::vec3 p, int m)
		: mPosition(p)
		, mMaterialIndex(m)
	{}
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

	int materialIndex;
	TriangleMesh(const Vertex& _a, const Vertex& _b, const Vertex& _c)
		: TriangleMesh(_a, _b, _c, -1)
	{}

	TriangleMesh(const Vertex& _a, const Vertex& _b, const Vertex& _c, int _materialIndex)
		: a(_a)
		, b(_b)
		, c(_c)
		, materialIndex(_materialIndex)
	{}

	Vertex getPoint(const glm::vec3& weight)const
	{
		auto w = weight;
		Vertex v;
//		v.mTexcoord = w.x * a.mTexcoord + w.y * b.mTexcoord + w.z * c.mTexcoord;
		v.mPosition = w.x * a.mPosition + w.y * b.mPosition + w.z * c.mPosition;
		{
			glm::vec3 pa = a.mPosition;
			glm::vec3 pb = b.mPosition;
			glm::vec3 pc = c.mPosition;
			v.mNormal = Triangle(pa, pb, pc).getNormal();
		}
		{
// 			auto tb = DrawHelper::createOrthoNormalBasis({ v.mNormal });
// 			v.mTangent = std::get<0>(tb)[0];
// 			v.mBinormal = std::get<1>(tb)[0];
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
	std::vector<Vertex>		mVertex;
	std::vector<glm::ivec3>	mIndex;

	Material mMaterial;

};

struct Model
{
	std::vector<Mesh> mMesh;
	std::vector<Material>	mMaterial;

	glm::mat4 mWorld;

	Model()
		: mWorld(1.f)
	{
	}

};

struct ModelList
{
	std::list<Model> mData;
};


struct Buffer {
	struct Command : public DrawElementsIndirectCommand
	{
//		int beginIndex;
	};
	enum BufferObject {
		BO_VERTEX,
		BO_NORMAL,
		BO_ELEMENT,
		BO_MATERIAL,
		BO_INDIRECT,
		BO_LS_VERTEX,
		BO_LS_NORMAL,
		BO_LS_ELEMENT,
		BO_LS_MATERIAL,
		BO_LS_INDIRECT,
		BO_MAX,
	};
	std::array<unsigned, BO_MAX> mBO;
	std::array<unsigned, 2> mVAO;
	int drawCount;
	int lightSourceDrawCount;
	int lightSourceElementCount;
	Buffer()
		: drawCount(0)
		, lightSourceDrawCount(0)
		, lightSourceElementCount(0)
	{
		glCreateBuffers(mBO.size(), mBO.data());
		glCreateVertexArrays(mVAO.size(), mVAO.data());
		{
			glVertexArrayElementBuffer(mVAO[0], mBO[BO_ELEMENT]);

			std::vector<unsigned> vbo = {
				mBO[BO_VERTEX],
				mBO[BO_NORMAL],
			};
			std::vector<GLintptr> offset = {
				0,
				0,
			};
			std::vector<GLsizei> stride = {
				sizeof(VertexGPU),
				sizeof(glm::vec3)
			};
			glVertexArrayVertexBuffers(mVAO[0], 0, vbo.size(), vbo.data(), offset.data(), stride.data());
			glVertexArrayAttribBinding(mVAO[0], 0, 0);
			glVertexArrayAttribFormat(mVAO[0], 0, 3, GL_FLOAT, GL_FALSE, 0);
			glEnableVertexArrayAttrib(mVAO[0], 0);
			glVertexArrayAttribBinding(mVAO[0], 1, 1);
			glVertexArrayAttribFormat(mVAO[0], 1, 3, GL_FLOAT, GL_FALSE, 16);
			glEnableVertexArrayAttrib(mVAO[0], 1);
			glVertexArrayAttribBinding(mVAO[0], 2, 0);
			glVertexArrayAttribIFormat(mVAO[0], 2, 1, GL_UNSIGNED_INT, 12);
			glEnableVertexArrayAttrib(mVAO[0], 2);
		}
	}

	~Buffer() 
	{
		if (mVAO[0] != 0)
		{
			glDeleteVertexArrays(mVAO.size(), mVAO.data());
			mVAO.fill(0);
		}
		if (mBO[0] != 0)
		{
			glDeleteBuffers(mBO.size(), mBO.data());
			mBO.fill(0);
		}
	}

	void setup(const ModelList& list)
	{
		{
			auto data = alignData(list);

			auto& vertex = std::get<ADI_VERTEX>(data);
			auto& normal = std::get<ADI_NORMAL>(data);
			auto& element = std::get<ADI_ELEMENT>(data);
			auto& material = std::get<ADI_MATERIAL>(data);
			auto& cmd = std::get<ADI_COMMADN>(data);
// 			glNamedBufferSubData(mBO[BO_VERTEX], 0, vertex.size() * sizeof(decltype(std::remove_reference<decltype(vertex)>::type())::value_type), vertex.data());
// 			glNamedBufferSubData(mBO[BO_NORMAL], 0, normal.size() * sizeof(decltype(std::remove_reference<decltype(normal)>::type())::value_type), normal.data());
// 			glNamedBufferSubData(mBO[BO_ELEMENT], 0, element.size() * sizeof(decltype(std::remove_reference<decltype(element)>::type())::value_type), element.data());
// 			glNamedBufferSubData(mBO[BO_MATERIAL], 0, material.size() * sizeof(decltype(std::remove_reference<decltype(material)>::type())::value_type), material.data());
// 			glNamedBufferSubData(mBO[BO_INDIRECT], 0, cmd.size() * sizeof(decltype(std::remove_reference<decltype(cmd)>::type())::value_type), cmd.data());
			glNamedBufferData(mBO[BO_VERTEX], vertex.size() * sizeof(decltype(std::remove_reference<decltype(vertex)>::type())::value_type), vertex.data(), GL_DYNAMIC_COPY);
			glNamedBufferData(mBO[BO_NORMAL], normal.size() * sizeof(decltype(std::remove_reference<decltype(normal)>::type())::value_type), normal.data(), GL_DYNAMIC_COPY);
			glNamedBufferData(mBO[BO_ELEMENT], element.size() * sizeof(decltype(std::remove_reference<decltype(element)>::type())::value_type), element.data(), GL_DYNAMIC_COPY);
			glNamedBufferData(mBO[BO_MATERIAL], material.size() * sizeof(decltype(std::remove_reference<decltype(material)>::type())::value_type), material.data(), GL_DYNAMIC_COPY);
			glNamedBufferData(mBO[BO_INDIRECT], cmd.size() * sizeof(decltype(std::remove_reference<decltype(cmd)>::type())::value_type), cmd.data(), GL_STATIC_DRAW);

			drawCount = cmd.size();
		}
	}

	void setupLight(const ModelList& list)
	{
		{
			{
				auto size = sizeof(VertexGPU) * 1000;
				glNamedBufferData(mBO[BO_LS_VERTEX], size, nullptr, GL_STATIC_DRAW);
				//			mTrianglePtr = reinterpret_cast<TriangleMesh*>(glMapNamedBufferRange(mBO[0], 0, size, mapFlag));
			}
			{
				auto size = sizeof(glm::vec3) * 1000;
				glNamedBufferData(mBO[BO_LS_NORMAL], size, nullptr, GL_STATIC_DRAW);
				//			mTrianglePtr = reinterpret_cast<TriangleMesh*>(glMapNamedBufferRange(mBO[0], 0, size, mapFlag));
			}
			{
				auto size = sizeof(glm::ivec3) * 1000;
				glNamedBufferData(mBO[BO_LS_ELEMENT], size, nullptr, GL_STATIC_DRAW);
			}

			{
				auto size = sizeof(Material) * 100;
				glNamedBufferData(mBO[BO_LS_MATERIAL], size, nullptr, GL_STATIC_DRAW);
				//			mMaterialPtr = reinterpret_cast<Material*>(glMapNamedBufferRange(mBO[1], 0, size, mapFlag));
			}
			glNamedBufferData(mBO[BO_LS_INDIRECT], sizeof(DrawElementsIndirectCommand) * 100, nullptr, GL_STATIC_DRAW);

		}
		{
			auto data = alignData(list);

			auto& vertex = std::get<ADI_VERTEX>(data);
			auto& normal = std::get<ADI_NORMAL>(data);
			auto& element = std::get<ADI_ELEMENT>(data);
			auto& material = std::get<ADI_MATERIAL>(data);
			auto& cmd = std::get<ADI_COMMADN>(data);
			glNamedBufferSubData(mBO[BO_LS_VERTEX], 0, vertex.size() * sizeof(decltype(std::remove_reference<decltype(vertex)>::type())::value_type), vertex.data());
			glNamedBufferSubData(mBO[BO_LS_NORMAL], 0, normal.size() * sizeof(decltype(std::remove_reference<decltype(normal)>::type())::value_type), normal.data());
			glNamedBufferSubData(mBO[BO_LS_ELEMENT], 0, element.size() * sizeof(decltype(std::remove_reference<decltype(element)>::type())::value_type), element.data());
			glNamedBufferSubData(mBO[BO_LS_MATERIAL], 0, material.size() * sizeof(decltype(std::remove_reference<decltype(material)>::type())::value_type), material.data());
			glNamedBufferSubData(mBO[BO_LS_INDIRECT], 0, cmd.size() * sizeof(decltype(std::remove_reference<decltype(cmd)>::type())::value_type), cmd.data());
			lightSourceDrawCount = cmd.size();
		}
	}

	void draw()
	{
		glBindVertexArray(mVAO[0]);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mBO[BO_INDIRECT]);
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, drawCount, sizeof(Command));
	}
private:
	using AlignData = std::tuple <std::vector<VertexGPU>, std::vector<glm::vec3>, std::vector<glm::ivec3>, std::vector<Material>, std::vector<Command>>;
	enum AlignDataIndex{
		ADI_VERTEX,
		ADI_NORMAL,
		ADI_ELEMENT,
		ADI_MATERIAL,
		ADI_COMMADN,
	};
	AlignData alignData(const ModelList &list)
	{
		int vcount = 0;
		int ecount = 0;
		int mcount = 0;
		for (auto& model : list.mData)
		{
			for (auto& mesh : model.mMesh)
			{
				vcount += mesh.mVertex.size();
				ecount += mesh.mIndex.size();
			}
			mcount += model.mMaterial.size();
		}

		std::vector<Command> cmd;
		std::vector<VertexGPU> vertex;
		std::vector<glm::vec3> normal;
		std::vector<glm::ivec3> element;
		std::vector<Material> material;
		vertex.reserve(vcount);
		normal.reserve(vcount);
		element.reserve(ecount);
		material.reserve(mcount);
		for (auto& model : list.mData)
		{
			auto moffset = material.size();
			material.insert(material.end(), model.mMaterial.begin(), model.mMaterial.end());
			for (auto& mesh : model.mMesh)
			{
				auto voffset = vertex.size();
				auto eoffset = element.size();
				auto v = mesh.mVertex;
				std::for_each(v.begin(), v.end(), [=](Vertex& _v) {_v.mMaterialIndex += moffset; });
				for (auto& _v : v)
				{
					vertex.emplace_back(_v.mPosition, _v.mMaterialIndex);
					normal.emplace_back(_v.mNormal);
				}
				for (auto& _e : mesh.mIndex)
				{
					element.emplace_back(_e + glm::ivec3(voffset));
				}
				Command c;
				c.baseInstance = 0;
				c.baseVertex = 0;
				c.instanceCount = 1;
				c.firstIndex = cmd.empty() ? 0 : (cmd.back().firstIndex + cmd.back().count);
				c.count = ((element.size() - eoffset)) * 3;
				cmd.push_back(c);
			}
		}

		return std::make_tuple(vertex, normal, element, material, cmd);
	}

};

struct UserData
{
	std::mt19937 mRandomEngine;
	std::vector<Photon> mPhoton;

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


	struct Scene{
	private:

		ModelList mModelList;
		ModelList mLightList;
		Buffer mBuffer;
		glm::vec4 mBackgroundColor;
	public:
		Scene()
			: mBackgroundColor(0.f, 0.f, 0.f, 1.f)
		{}
		void push(const Model& model);
		void pushLight(const Model& model);
		void setup();

		const ModelList& getModelList()const { return mModelList; }
		glm::vec4 radiance(const Ray& ray, int depth = 0);

		Buffer& getBuffer() { return mBuffer; }
		void drawScene() { mBuffer.draw(); }
	};

	Scene mScene;
	ThreadPool mThreadPool;

	PhotonMap();
	void setupScene();

};

}


