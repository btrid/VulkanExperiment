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

enum class DataType {
	FLOAT = GL_FLOAT,
	INT = GL_UNSIGNED_INT,
	SHORT = GL_UNSIGNED_SHORT,
	BYTE = GL_UNSIGNED_BYTE,
	DOUBLE = GL_DOUBLE,
};

struct VertexBuffer
{
	struct Attribute
	{
		int			m_attr_index;
		DataType	m_type;
		int			m_size;
		int			m_offset;
		bool		m_is_normalize;

		Attribute() {}
		Attribute(int attr_index, DataType type, int size, int offset, bool normalize)
			: m_attr_index(attr_index), m_type(type), m_size(size), m_offset(offset), m_is_normalize(normalize)
		{}
	};

	unsigned m_vbo;
	unsigned m_bind;
	unsigned m_offset;
	unsigned m_stride;
	std::vector<Attribute> m_attr;

};

struct GPUResource 
{
//	static std::list<GPUResource> s_delete_list;
};

// struct Buffer : public GPUResource 
// {
// 
// };
struct VertexLayout : public GPUResource
{
	static void deleter(unsigned* vao) { glDeleteVertexArrays(1, vao); }
	using smart_ptr = std::unique_ptr<unsigned, std::function<void(unsigned*)>>;

	static VertexLayout Create(const char* label) {
		unsigned vao;
//		glGetObjectLabel()
		glCreateVertexArrays(1, &vao);
		glObjectLabel(GL_VERTEX_ARRAY, vao, -1, label);
		VertexLayout vertex_layout;
//		vertex_layout.m_vao = std::make_unique<unsigned, std::function<void(unsigned*)>>(new unsigned(vao), &VertexLayout::deleter);
		vertex_layout.m_vao = std::unique_ptr<unsigned, std::function<void(unsigned*)>>(new unsigned(vao), &VertexLayout::deleter);
		return vertex_layout;
	}

//private:
	VertexLayout() = default;
public:
	std::unique_ptr<unsigned, std::function<void(unsigned*)>> m_vao;
	int m_first;
	std::vector<VertexBuffer> m_buffers;

	unsigned getHandle()const { return m_vao ? *m_vao : 0; }
};

namespace pm4
{
	struct Vertex;
	struct Material;
	struct Model;


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
	float m_specular; //!< Œõ‘ò
	float m_rough; //!< ‚´‚ç‚´‚ç
	float m_metalic;
	std::uint64_t	m_diffuse_tex;
	std::uint64_t	m_ambient_tex;
	std::uint64_t	m_specular_tex;
	std::uint64_t	m_emissive_tex;
	Material()
		: mDiffuse(0.f)
		, mEmissive(0.f)
		, m_diffuse(0.5f)
		, m_specular(0.f)
		, m_rough(0.f)
		, m_metalic(0.f)
		, m_diffuse_tex(0)
		, m_ambient_tex(0)
		, m_specular_tex(0)
		, m_emissive_tex(0)
	{

	}

	struct SamplingResult
	{
		// photonmap p181
		// ƒ‚ƒ“ƒeƒJƒ‹ƒÏ•ª‚Ìimportance_sampling‚Ì‚½‚ß‚Ì
		// Šm—¦–§“xŠÖ” probability_density_function
		float m_pdf;
		// ‘o•ûŒü”½ŽË—¦•ª•zŠÖ” Bidirectional Reflectance Distribution Function
		glm::vec3 m_brdf;

		// ”½ŽË•ûŒü
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
			// ŠgŽU”½ŽË
			return 1;
		}
		else if (probabry < m_diffuse + m_specular)
		{
			// ‹¾–Ê”½ŽË
			return 2;
		}

		//	‹zŽû
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
	glm::vec2 m_texcoord;

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
	glm::vec3 m_position;
	glm::vec3 m_normal;
	int m_material_index;
	glm::vec2 m_texcoord;
	glm::vec2 m_texcoord1;

	VertexGPU() = default;
	VertexGPU(const glm::vec3& p, const glm::vec3& n, int m, const glm::vec2& t0)
		: m_position(p)
		, m_normal(n)
		, m_material_index(m)
		, m_texcoord(t0)
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


struct RasterizerState
{

};

struct Buffer {
	struct Command : public DrawElementsIndirectCommand
	{
//		int beginIndex;
	};
	enum BufferObject {
		BO_VERTEX,
		BO_ELEMENT,
		BO_MATERIAL,
		BO_INDIRECT,
		BO_LS_VERTEX,
		BO_LS_ELEMENT,
		BO_LS_MATERIAL,
		BO_LS_INDIRECT,
		BO_MAX,
	};
	std::array<unsigned, BO_MAX> mBO;
	VertexLayout m_vertex_layout;
	int drawCount;
	int lightSourceDrawCount;
	int lightSourceElementCount;
	Buffer()
		: drawCount(0)
		, lightSourceDrawCount(0)
		, lightSourceElementCount(0)
	{

		glCreateBuffers(mBO.size(), mBO.data());
//		glCreateVertexArrays(mVAO.size(), mVAO.data());
		{

			m_vertex_layout = VertexLayout::Create("VoxelMesh");
			m_vertex_layout.m_first = 0;
			{
				VertexBuffer buffer;
				buffer.m_vbo = mBO[BO_VERTEX];
				buffer.m_bind = 0;
				buffer.m_offset = 0;
				buffer.m_stride = sizeof(VertexGPU);
				buffer.m_attr.emplace_back(0, DataType::FLOAT,	3,  0, false);
				buffer.m_attr.emplace_back(1, DataType::FLOAT,	3, 12, false);
				buffer.m_attr.emplace_back(2, DataType::INT,	1, 24, false);
				buffer.m_attr.emplace_back(3, DataType::FLOAT,	2, 28, false);
				buffer.m_attr.emplace_back(4, DataType::FLOAT,	2, 36, false);
				m_vertex_layout.m_buffers.emplace_back(buffer);
			}

			{
				glVertexArrayElementBuffer(m_vertex_layout.getHandle(), mBO[BO_ELEMENT]);

			}
			{
				
				std::vector<unsigned> vbo(m_vertex_layout.m_buffers.size());
				std::vector<GLintptr> offset(m_vertex_layout.m_buffers.size());
				std::vector<GLsizei> stride(m_vertex_layout.m_buffers.size());

				int index = 0;
				for (auto& buffer : m_vertex_layout.m_buffers)
				{
					vbo[index] = buffer.m_vbo;
					offset[index] = buffer.m_offset;
					stride[index] = buffer.m_stride;
					index++;
				}

				glVertexArrayVertexBuffers(m_vertex_layout.getHandle(), m_vertex_layout.m_first, vbo.size(), vbo.data(), offset.data(), stride.data());
			}

			{
				int bindIndex = m_vertex_layout.m_first;
				for (const auto& buffer : m_vertex_layout.m_buffers)
				{
					for (const auto& attr : buffer.m_attr)
					{
						glVertexArrayAttribBinding(m_vertex_layout.getHandle(), attr.m_attr_index, bindIndex);
						switch (attr.m_type)
						{
						case DataType::FLOAT:
							glVertexArrayAttribFormat(m_vertex_layout.getHandle(), attr.m_attr_index, attr.m_size, static_cast<int>(attr.m_type), attr.m_is_normalize, attr.m_offset);
							break;
						case DataType::INT:
						case DataType::SHORT:
						case DataType::BYTE:
							glVertexArrayAttribIFormat(m_vertex_layout.getHandle(), attr.m_attr_index, attr.m_size, static_cast<int>(attr.m_type), attr.m_offset);
							break;
						case DataType::DOUBLE:
							glVertexArrayAttribLFormat(m_vertex_layout.getHandle(), attr.m_attr_index, attr.m_size, static_cast<int>(attr.m_type), attr.m_offset);
							break;
						default:
							assert(false);
							break;
						}
						glEnableVertexArrayAttrib(m_vertex_layout.getHandle(), attr.m_attr_index);
					}
					bindIndex++;
				}

			}
		}
	}

	~Buffer() 
	{
// 		if (mVAO[0] != 0)
// 		{
// 			glDeleteVertexArrays(mVAO.size(), mVAO.data());
// 			mVAO.fill(0);
// 		}
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
			auto& element = std::get<ADI_ELEMENT>(data);
			auto& material = std::get<ADI_MATERIAL>(data);
			auto& cmd = std::get<ADI_COMMADN>(data);
			glNamedBufferData(mBO[BO_VERTEX], vertex.size() * sizeof(decltype(std::remove_reference<decltype(vertex)>::type())::value_type), vertex.data(), GL_STATIC_DRAW);
			glNamedBufferData(mBO[BO_ELEMENT], element.size() * sizeof(decltype(std::remove_reference<decltype(element)>::type())::value_type), element.data(), GL_STATIC_DRAW);
			glNamedBufferData(mBO[BO_MATERIAL], material.size() * sizeof(decltype(std::remove_reference<decltype(material)>::type())::value_type), material.data(), GL_STATIC_DRAW);
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
			auto& element = std::get<ADI_ELEMENT>(data);
			auto& material = std::get<ADI_MATERIAL>(data);
			auto& cmd = std::get<ADI_COMMADN>(data);
			glNamedBufferSubData(mBO[BO_LS_VERTEX], 0, vertex.size() * sizeof(decltype(std::remove_reference<decltype(vertex)>::type())::value_type), vertex.data());
			glNamedBufferSubData(mBO[BO_LS_ELEMENT], 0, element.size() * sizeof(decltype(std::remove_reference<decltype(element)>::type())::value_type), element.data());
			glNamedBufferSubData(mBO[BO_LS_MATERIAL], 0, material.size() * sizeof(decltype(std::remove_reference<decltype(material)>::type())::value_type), material.data());
			glNamedBufferSubData(mBO[BO_LS_INDIRECT], 0, cmd.size() * sizeof(decltype(std::remove_reference<decltype(cmd)>::type())::value_type), cmd.data());
			lightSourceDrawCount = cmd.size();
		}
	}

	void draw()
	{
//		glBindVertexArray(mVAO[0]);
		glBindVertexArray(m_vertex_layout.getHandle());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mBO[BO_INDIRECT]);
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, drawCount, sizeof(Command));
	}
private:
	using AlignData = std::tuple <std::vector<VertexGPU>, std::vector<glm::ivec3>, std::vector<Material>, std::vector<Command>>;
	enum AlignDataIndex{
		ADI_VERTEX,
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
		std::vector<glm::ivec3> element;
		std::vector<Material> material;
		vertex.reserve(vcount);
		element.reserve(ecount);
		material.reserve(mcount);
		auto moffset = 0;
		for (auto& model : list.mData)
		{
			material.insert(material.end(), model.mMaterial.begin(), model.mMaterial.end());
			for (auto& mesh : model.mMesh)
			{
				auto voffset = vertex.size();
				auto eoffset = element.size();
				auto v = mesh.mVertex;
				std::for_each(v.begin(), v.end(), [=](Vertex& _v) {_v.mMaterialIndex += moffset; });
				for (auto& _v : v)
				{
					vertex.emplace_back(_v.mPosition, _v.mNormal, _v.mMaterialIndex, _v.m_texcoord);
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
			moffset += material.size();
		}

		return std::make_tuple(vertex, element, material, cmd);
	}

};

struct UserData
{
	std::mt19937 mRandomEngine;

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


