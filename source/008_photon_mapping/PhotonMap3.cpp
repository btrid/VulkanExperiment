#include "PhotonMap3.h"

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
#include "FrameBuffer.h"
#include "PhotonMap2Define.h"

namespace pm3
{

Mesh createMesh(int type)
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

	Mesh mesh;
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
// 			mesh.mVertex[i].mTangent = std::get<0>(tb)[i];
// 			mesh.mVertex[i].mBinormal = std::get<1>(tb)[i];
// 			mesh.mVertex[i].mTexcoord = glm::vec3(0.f);
		mesh.mVertex[i].mMaterialIndex = 0;
	}

	return mesh;
}
struct BrickParam
{
	glm::ivec3 size0;
	float p1;
	glm::ivec3 size1;
	int scale1;
	
	glm::vec3 areaMin;
	float p3;
	glm::vec3 areaMax;
	float p4;
};


PhotonMap::PhotonMap()
{

	ShaderProgram::AddHeader("../GL/PhotonMap2Define.h", "/PMDefine.glsl"); //やめる
	ShaderProgram::AddHeader("../Resource/Shader/PM3/Brick.glsl", "/Brick.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM3/PM.glsl", "/PM.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM3/Shape.glsl", "/Shape.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM3/Marching.glsl", "/Marching.glsl");

	setupScene();

	int width = 640;
	int height = 480;

	Camera camera;
	{
		vec3 eye = vec3(0., 30., -230.);
		vec3 target = vec3(0., 10., 0.);
		camera.lookAt(eye, target, glm::vec3(0.f, 1.f, 0.f));
		camera.perspective(glm::radians(60.f), width, height, 0.1f, 100000.f);

	}
	Texture render;
	{
		std::vector<vec4> color(width * height);
		render.create(TextureTarget::TEXTURE_2D);
		render.image2D(0, InternalFormat::RGBA32F, width, height, Format::RGBA, PixelType::FLOAT);
	}

	unsigned BrickBO;
	BrickParam p;
	{
		p.size0 = glm::ivec3(BRICK_SIZE);
		p.size1 = glm::ivec3(BRICK_SUB_SIZE);
		p.scale1 = BRICK_SCALE1;
		p.areaMin = glm::BRICK_AREA_MIN;
		p.areaMax = glm::BRICK_AREA_MAX;

		glCreateBuffers(1, &BrickBO);
		glNamedBufferData(BrickBO, sizeof(BrickParam), &p, GL_STATIC_COPY);

	}

	ProgramPipeline brickRender;
	VertexProgram brickRenderVS;
	FragmentProgram brickRenderFS;
	brickRenderVS.createFromFile("../Resource/Shader/PM3/BrickRender.vert");
	brickRenderFS.createFromFile("../Resource/Shader/PM3/BrickRender.frag");
	brickRender.useProgramStage(brickRenderVS, ShaderBit::VERTEX_SHADER_BIT);
	brickRender.useProgramStage(brickRenderFS, ShaderBit::FRAGMENT_SHADER_BIT);

	unsigned BrickVAO;
	unsigned BrickVBO;
	{
		glCreateBuffers(1, &BrickVBO);
		std::vector<glm::vec3> box;
		DrawHelper::GetBox(box);
		glNamedBufferData(BrickVBO, sizeof(glm::vec3)*box.size(), box.data(), GL_STATIC_DRAW);

		glCreateVertexArrays(1, &BrickVAO);
		glVertexArrayVertexBuffer(BrickVAO, 0, BrickVBO, 0, sizeof(glm::vec3));
		glVertexArrayAttribBinding(BrickVAO, 0, 0);
		glEnableVertexArrayAttrib(BrickVAO, 0);
		glVertexArrayAttribFormat(BrickVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
	}

	Texture BrickMap0;
	{
		glm::ivec3 brickSize(BRICK_SIZE);
		BrickMap0.create(TextureTarget::TEXTURE_3D);
		BrickMap0.image3D(0, InternalFormat::R8UI, brickSize.x, brickSize.y, brickSize.z, Format::RED_INTEGER, PixelType::UNSIGNED_BYTE, nullptr);
	}

	enum TriangleBO {
		TBO_LL_INFO,
		TBO_LL_HEAD,
		TBO_LL,
		TBO_ATOMIC,
		TBO_NUM,
	};
	struct _TriangleLL
	{
		std::int32_t next;
		std::int32_t drawID;
		std::int32_t instanceID;
		std::int32_t _p1;

		std::int32_t index[3];
		std::int32_t _p2;

	};

	using TriangleLL = _TriangleLL;
	std::array<unsigned, TBO_NUM> triangleBO;
	glCreateBuffers(triangleBO.size(), triangleBO.data());
	glNamedBufferData(triangleBO[TBO_LL_HEAD], sizeof(std::uint32_t) * BRICK_SUB_TOTAL, nullptr, GL_STREAM_COPY);
	glNamedBufferData(triangleBO[TBO_LL], sizeof(TriangleLL) * 1000 * 1000 * 3, nullptr, GL_STREAM_COPY);
	glNamedBufferData(triangleBO[TBO_ATOMIC], sizeof(std::uint32_t), nullptr, GL_STREAM_COPY);

	ProgramPipeline triangleClear;
	ComputeProgram triangleClearCS;
	triangleClearCS.createFromFile("../Resource/Shader/PM3/TriangleClear.comp");
	triangleClear.useProgramStage(triangleClearCS, ShaderBit::COMPUTE_SHADER_BIT);

	ProgramPipeline voxel;
	VertexProgram voxelVS;
	GeometryProgram voxelGS;
	FragmentProgram voxelFS;
	voxelVS.createFromFile("../Resource/Shader/PM3/Voxelize.vert");
	voxelGS.createFromFile("../Resource/Shader/PM3/Voxelize.geom");
	voxelFS.createFromFile("../Resource/Shader/PM3/Voxelize.frag");
	voxel.useProgramStage(voxelVS, ShaderBit::VERTEX_SHADER_BIT);
	voxel.useProgramStage(voxelGS, ShaderBit::GEOMETRY_SHADER_BIT);
	voxel.useProgramStage(voxelFS, ShaderBit::FRAGMENT_SHADER_BIT);

	unsigned PhotonLL;
	glCreateBuffers(1, &PhotonLL);

	enum PhotonBO{
		PBO_PHOTON_COUNT,
		PBO_LL_HEAD,
		PBO_LL,
		PBO_DATA,
		PBO_PHOTON_BOUNCE_COUNT,
		PBO_NUM,
	};
	struct Bounce 
	{
		int count;
		int startCount;
		int calced;
		int _p;
	};
	std::array<unsigned, PBO_NUM> photonBO;
	glCreateBuffers(photonBO.size(), photonBO.data());
	glNamedBufferData(photonBO[PBO_PHOTON_COUNT], sizeof(std::uint32_t), nullptr, GL_STREAM_COPY);
	glNamedBufferData(photonBO[PBO_LL_HEAD], sizeof(std::uint32_t) * BRICK_SUB_SIZE *BRICK_SUB_SIZE *BRICK_SUB_SIZE, nullptr, GL_STREAM_COPY);
	glNamedBufferData(photonBO[PBO_LL], sizeof(std::uint32_t) * 1000 * 1000 * 10, nullptr, GL_STREAM_COPY);
	glNamedBufferData(photonBO[PBO_DATA], sizeof(Photon) * 1000 * 1000 * 10, nullptr, GL_STREAM_COPY);
	glNamedBufferData(photonBO[PBO_PHOTON_BOUNCE_COUNT], sizeof(Bounce)*5, nullptr, GL_STREAM_COPY);

	ProgramPipeline photonMake;
	ComputeProgram photonMakeCS;
	photonMakeCS.createFromFile("../Resource/Shader/PM3/PhotonMake.comp");
	photonMake.useProgramStage(photonMakeCS, ShaderBit::COMPUTE_SHADER_BIT);

	ProgramPipeline photonMapping;
	ComputeProgram photonMappingCS;
	photonMappingCS.createFromFile("../Resource/Shader/PM3/PhotonMapping.comp");
	photonMapping.useProgramStage(photonMappingCS, ShaderBit::COMPUTE_SHADER_BIT);

	ProgramPipeline photonBounce;
	ComputeProgram photonBounceCS;
	photonBounceCS.createFromFile("../Resource/Shader/PM3/PhotonMappingBounce.comp");
	photonBounce.useProgramStage(photonBounceCS, ShaderBit::COMPUTE_SHADER_BIT);

	ProgramPipeline photonRendering;
	ComputeProgram photonRenderingCS;
	photonRenderingCS.createFromFile("../Resource/Shader/PM3/PhotonRendering.comp");
	photonRendering.useProgramStage(photonRenderingCS, ShaderBit::COMPUTE_SHADER_BIT);

	enum IndirectBO {
		IBO_MAPPING,
		IBO_RENDERING,
		IBO_MAX,
	};

	std::array<unsigned, IBO_MAX> indirectBO;
	{
		glCreateBuffers(indirectBO.size(), indirectBO.data());
		{
			glm::ivec3 ibo{ BRICK_SUB_SIZE, 4, 1 };
			glNamedBufferData(indirectBO[IBO_MAPPING], sizeof(glm::ivec3), &ibo, GL_STATIC_DRAW);
		}
		{
			glm::ivec3 ibo{ width/32, height/32, 1 };
			glNamedBufferData(indirectBO[IBO_RENDERING], sizeof(glm::ivec3), &ibo, GL_STATIC_DRAW);
		}
	}

	while (!Renderer::order()->isClose())
	{
		auto b = std::chrono::system_clock::now();

		camera.control(0.16f);

		// テクスチャ初期化
		{
			// シェーダで初期化
			{
				std::uint32_t c[4] = { 0x0, 0x0, 0x0, 0x0 };
				glClearTexImage(BrickMap0.getHandle(), 0, Format::RED_INTEGER, PixelType::UNSIGNED_BYTE, c);
				glClearTexImage(render.getHandle(), 0, Format::RGBA, PixelType::FLOAT, c);
				
			}
			{
				// 一定以上bufferが大きいと実行時エラー？
#if defined(USE_SHADER) || 1
// 
				triangleClear.bind();
				{
					std::vector<unsigned> buffers = {
						photonBO[PBO_LL_HEAD],
					};
					glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());
					std::vector<unsigned> uniforms = {
						BrickBO,
					};
					glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());
					glDispatchCompute(BRICK_SUB_SIZE / 64, BRICK_SUB_SIZE / 16, BRICK_SUB_SIZE);

					buffers = {
						triangleBO[TBO_LL_HEAD],
					};
					glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());
					glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());
					glDispatchCompute(BRICK_SUB_SIZE / 64, BRICK_SUB_SIZE / 16, BRICK_SUB_SIZE);
				}
				
#else
				std::uint32_t c[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
				glClearNamedBufferData(photonBO[PBO_LL_HEAD], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
				glClearNamedBufferData(photonBO[PBO_LL], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, nullptr);
				glClearNamedBufferData(triangleBO[TBO_LL], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
#endif
			}

			std::uint32_t c[1] = { 0x0 };
			glClearNamedBufferData(photonBO[PBO_PHOTON_COUNT], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
			glClearNamedBufferData(photonBO[PBO_PHOTON_BOUNCE_COUNT], InternalFormat::RGBA32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
			glClearNamedBufferData(triangleBO[TBO_ATOMIC], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
		}
//		if(0)
		{
			// Voxel生成
			glViewportIndexedf(0, 0, 0, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
			glViewportIndexedf(1, 0, 0, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
			glViewportIndexedf(2, 0, 0, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
//			glViewport(0, 0, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
			glDisable(GL_CULL_FACE);
			glDepthMask(GL_FALSE);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA_TEST);
			glClearColor(0.2f, 0.2f, 0.2f, 1.f);
			Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			voxel.bind();
			std::vector<unsigned> textures = {
				BrickMap0.getHandle(),
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				triangleBO[TBO_LL_HEAD],
				triangleBO[TBO_LL],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());

			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, triangleBO[TBO_ATOMIC]);

			mScene->drawScene();

		}
		

		auto c1D3D = [](int in, glm::ivec3 size)->auto
		{
			int x = in % size.x;
			int y = (in / size.x) % size.y;
			int z = in / (size.y * size.x);

			return glm::ivec3(x, y, z);
		};
		if (0)
		{
			std::vector<char> a(BRICK_SIZE * BRICK_SIZE * BRICK_SIZE);
			glGetTextureImage(BrickMap0.getHandle(), 0, Format::RED_INTEGER, PixelType::UNSIGNED_BYTE, a.size() * 1, a.data());
			printf("Brick0\n");

			for (int i = 0; i < a.size(); i++)
			{
				if (a.data()[i] != 0)
				{
					auto vec = c1D3D(i, glm::ivec3(BRICK_SIZE));
					printf("[%2d,%2d,%2d] = %d\n", vec.x, vec.y, vec.z, a.data()[i]);
				}
			}
		}
		if(0)
		{
			glFinish();
			std::int32_t a = 0;
			glGetNamedBufferSubData(triangleBO[TBO_ATOMIC], 0, 4, &a);
			std::vector<TriangleLL> data(a);
			glGetNamedBufferSubData(triangleBO[TBO_LL], 0, data.size() * sizeof(TriangleLL), data.data());
			printf("triangleBO[TBO_LL]\n");
			for (int i = 0; i < data.size(); i++)
			{				
				auto& aaa = data.data()[i];
				printf("[i=%5d]next=%5d, draw=%5d, instance=%5d, v[0, 1, 2]=%5d, %5d, %5d\n", i, aaa.next, aaa.drawID, aaa.instanceID, aaa.index[0], aaa.index[1], aaa.index[2]);
			}

		}
		if (0)
		{
			std::int32_t a = 0;
			std::int32_t b = 0;
			glGetNamedBufferSubData(triangleBO[TBO_ATOMIC], 0, 4, &a);
			glGetNamedBufferSubData(photonBO[PBO_PHOTON_COUNT], 0, 4, &b);
			printf("TriangleLL Num = %d, PhotonLL Num = %d\n", a, b);
		}

		// PhotonMapping まずはLightSourceから放射
//		if (0)
		{
			photonMapping.bind();

			std::vector<unsigned> textures = {
				BrickMap0.getHandle(),
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				mScene->getBuffer().mBO[Buffer::BO_LS_VERTEX],
				mScene->getBuffer().mBO[Buffer::BO_LS_ELEMENT],
				mScene->getBuffer().mBO[Buffer::BO_LS_MATERIAL],
				mScene->getBuffer().mBO[Buffer::BO_VERTEX],
				mScene->getBuffer().mBO[Buffer::BO_ELEMENT],
				mScene->getBuffer().mBO[Buffer::BO_MATERIAL],
				triangleBO[TBO_LL_HEAD],
				triangleBO[TBO_LL],
				photonBO[PBO_DATA],
				photonBO[PBO_LL_HEAD],
				photonBO[PBO_LL],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());

			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, photonBO[PBO_PHOTON_COUNT]);
			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBO[IBO_MAPPING]);
			glDispatchComputeIndirect(0);
			glFinish();
		}

		// Bounce
		glCopyNamedBufferSubData(photonBO[PBO_PHOTON_COUNT], photonBO[PBO_PHOTON_BOUNCE_COUNT], 0, 0, 4);
		for (int i = 0; i < 3; i++)
		{
			// 事前準備
			glCopyNamedBufferSubData(photonBO[PBO_PHOTON_COUNT], photonBO[PBO_PHOTON_BOUNCE_COUNT], 0, (i + 1) * sizeof(Bounce)+4, 4);

			photonBounce.bind();
			std::vector<unsigned> textures = {
				BrickMap0.getHandle(),
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				mScene->getBuffer().mBO[Buffer::BO_VERTEX],
				mScene->getBuffer().mBO[Buffer::BO_ELEMENT],
				mScene->getBuffer().mBO[Buffer::BO_MATERIAL],
				triangleBO[TBO_LL_HEAD],
				triangleBO[TBO_LL],
				photonBO[PBO_DATA],
				photonBO[PBO_LL_HEAD],
				photonBO[PBO_LL],
				photonBO[PBO_PHOTON_BOUNCE_COUNT],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());

			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, photonBO[PBO_PHOTON_COUNT]);

			photonBounceCS.setUniform("gBounceCount", i+1);

			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBO[IBO_MAPPING]);
			glDispatchComputeIndirect(0);
			glFinish();

		}
		{

		}
//		if (0)
		{
			std::int32_t a = 0;
			std::int32_t b = 0;
			glGetNamedBufferSubData(triangleBO[TBO_ATOMIC], 0, 4, &a);
			glGetNamedBufferSubData(photonBO[PBO_PHOTON_COUNT], 0, 4, &b);
			printf("TriangleLL Num = %d, PhotonLL Num = %d\n", a, b);

			std::vector<Bounce> bounce(5);
			glGetNamedBufferSubData(photonBO[PBO_PHOTON_BOUNCE_COUNT], 0, sizeof(Bounce)*bounce.size(), bounce.data());
			for (auto& _b : bounce)
			{
				printf("count=%8d offset=%8d calced=%8d\n", _b.count, _b.startCount, _b.calced);
			}
		}

		if (0)
		{
			std::int32_t a = 0;
			glGetNamedBufferSubData(photonBO[PBO_PHOTON_COUNT], 0, 4, &a);
			std::vector<unsigned> head(BRICK_SUB_SIZE*BRICK_SUB_SIZE*BRICK_SUB_SIZE);
			glGetNamedBufferSubData(photonBO[PBO_LL_HEAD], 0, head.size() * sizeof(unsigned), head.data());
			std::vector<unsigned> link(a);
			glGetNamedBufferSubData(photonBO[PBO_LL], 0, a * sizeof(unsigned),link.data());
			for (int i = 0; i < head.size(); i++)
			{
				auto& d = head.data()[i];
				auto index = c1D3D(i, glm::ivec3(BRICK_SUB_SIZE));
				printf("[index=%5d,%5d,%5d] head = %5d\n", index.x, index.y, index.z, d);
				for (int l = d; l != -1; l = link.data()[l])
				{
					printf(" index = %5d\n", l);
				}
			}
			std::vector<Photon> data(a);
			glGetNamedBufferSubData(photonBO[PBO_DATA], 0, data.size() * sizeof(TriangleLL), data.data());
			printf("photonBO[PBO_DATA]\n");
			for (int i = 0; i < data.size(); i++)
			{
				auto& aaa = data.data()[i];
				auto d = aaa.getDir();
				printf("[i=%5d]pos=%6.1f, %6.1f, %6.1f, dir=%6.1f, %6.1f, %6.1f\n", i, aaa.mPos.x, aaa.mPos.y, aaa.mPos.z, d.x, d.y, d.z);
			}

		}

		// 画面からの光線
#define VOXEL_TEST 0
#if VOXEL_TEST==0

//		if(0)
		{
			photonRendering.bind();

			std::vector<unsigned> textures = {
				BrickMap0.getHandle(),
				render.getHandle(),
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				mScene->getBuffer().mBO[Buffer::BO_VERTEX],
				mScene->getBuffer().mBO[Buffer::BO_ELEMENT],
				mScene->getBuffer().mBO[Buffer::BO_MATERIAL],
				triangleBO[TBO_LL_HEAD],
				triangleBO[TBO_LL],
				photonBO[PBO_DATA],
				photonBO[PBO_LL_HEAD],
				photonBO[PBO_LL],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());


			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBO[IBO_RENDERING]);
			glDispatchComputeIndirect(0);

			photonRenderingCS.setUniform("eye", camera.getPosition());
			photonRenderingCS.setUniform("target", camera.getTarget());

			// 描画
			{
				glViewport(0, 0, width, height);
				glDepthMask(GL_FALSE);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				DrawHelper::Order()->drawTexture(render.getHandle());
			}
		}
#else
		{
			glViewport(0, 0, width, height);
			glDepthMask(GL_FALSE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			brickRender.bind();
			glBindVertexArray(BrickVAO);

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				triangleBO[TBO_LL_HEAD],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());

			brickRenderVS.setUniform("PV", camera.getProjection()*camera.getView());

			glDrawArraysInstanced(GL_LINES, 0, 36, BRICK_SUB_SIZE*BRICK_SUB_SIZE*BRICK_SUB_SIZE + 1);
		}
#endif


		Renderer::order()->swapBuffer();
		Renderer::order()->loopEvent();

		auto e = std::chrono::system_clock::now();
		printf("%lld.%03lld\n", std::chrono::duration_cast<std::chrono::seconds>(e - b).count(), std::chrono::duration_cast<std::chrono::milliseconds>(e - b).count()%1000);
	}

}

std::unique_ptr<Scene> SetupScene()
{
	auto mScene = std::make_unique<Scene>();
	if (0)
	{
		unsigned modelFlag = aiProcessPreset_TargetRealtime_MaxQuality
			// 		unsigned modelFlag = 0
			// 			| aiProcess_JoinIdenticalVertices
			// 			| aiProcess_ImproveCacheLocality
			// 			| aiProcess_LimitBoneWeights
			// 			| aiProcess_RemoveRedundantMaterials
			// //			| aiProcess_SplitLargeMeshes
			// 			| aiProcess_Triangulate
			// 			| aiProcess_SortByPType
			// 			| aiProcess_FindDegenerates
			// //			| aiProcess_CalcTangentSpace
			//				| aiProcess_FlipUVs
			;

		using namespace Assimp;
		Assimp::Importer importer;
//		std::string filepath = "../Resource/Tiny/tiny.x";
		std::string filepath = "../Resource/model/tigre_sumatra_sketchfab.obj";
		importer.ReadFile(filepath, modelFlag);
		const aiScene* scene = importer.GetScene();

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
				//				std::memcpy(&mat.mEmissive, &color, sizeof(color));
				//				aiMat->Get(AI_MATKEY_SHININESS, mat.mShininess);
				aiString str;
				aiTextureMapMode mapmode[3];
				aiTextureMapping mapping;
				unsigned uvIndex;
				if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					mat.m_diffuse_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_diffuse_tex);
				}
				if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					mat.m_ambient_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_ambient_tex);
				}
				if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					mat.m_specular_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_specular_tex);
				}
				if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					mat.m_emissive_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_emissive_tex);
				}

				//				if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
				// 					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
				// 					mat.mTextureNormal.mData = std::make_shared<std::vector<glm::vec4>>(data.mData);
				// 					mat.mTextureNormal.mSize = data.mSize;
				//				}

			}

		}

		std::vector<Mesh> meshes(scene->mNumMeshes);
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


			for (size_t i = 0; i < scene->mNumMeshes; i++)
			{
				aiMesh* ai_mesh = scene->mMeshes[i];
				_materialIndex[i] = ai_mesh->mMaterialIndex;
				auto& mesh = meshes[i];

				// ELEMENT_ARRAY_BUFFER
				// 三角メッシュとして読み込む
				mesh.mIndex.reserve(scene->mMeshes[i]->mNumFaces);
				for (size_t n = 0; n < ai_mesh->mNumFaces; n++) {
					mesh.mIndex.emplace_back(ai_mesh->mFaces[n].mIndices[0], ai_mesh->mFaces[n].mIndices[1], ai_mesh->mFaces[n].mIndices[2]);
				}

				// ARRAY_BUFFER
				mesh.mVertex.resize(scene->mMeshes[i]->mNumVertices);
				for (size_t n = 0; n < ai_mesh->mNumVertices; n++)
				{
					auto& v = mesh.mVertex[n];
					v.mPosition = glm::vec3(ai_mesh->mVertices[n].x, ai_mesh->mVertices[n].y, ai_mesh->mVertices[n].z);
					if (ai_mesh->HasNormals()) {
						//						vertex[i].mNormal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
					}
					if (ai_mesh->HasTextureCoords(0)) {
						v.m_texcoord = glm::vec2(ai_mesh->mTextureCoords[0][n].x, ai_mesh->mTextureCoords[0][n].y);
					}
					if (ai_mesh->HasTangentsAndBitangents())
					{
						// 						meshes[i].mVertex[i].mTangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
						// 						meshes[i].mVertex[i].mBinormal = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
					}
					v.mMaterialIndex = ai_mesh->mMaterialIndex;
				}
				std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](auto& v) { v.mPosition*200.f; });
			}
		}



		Model model;
		model.mMesh = meshes;
		model.mMaterial = material;
		model.mWorld = glm::translate(glm::vec3(0.f, -900.f, 0.f));
		mScene->push(model);
	}

	// ライト
	if(0)
	{
		Mesh mesh = createMesh(2);
		std::swap(mesh.mIndex[0][1], mesh.mIndex[0][2]);
		std::swap(mesh.mIndex[1][1], mesh.mIndex[1][2]);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition *= 50.f; p.mPosition.y = 88.f; p.mNormal = -p.mNormal; });
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mEmissive = glm::vec4(1.0f);
		material[0].mDiffuse = glm::vec4(1.0f);

		model.mWorld *= glm::translate(glm::vec3(0.f, 19.9f, 0.f));
		model.mWorld *= glm::rotate(glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(6.f, 1.f, 6.f));

		mScene->push(model);
		mScene->pushLight(model);

	}

	if (0)
	{
		// 中央のボックス
		Mesh mesh = createMesh(0);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition += glm::vec3(0.f, 0.5f, 0.f); p.mPosition *= 50.f; p.mPosition -= glm::vec3(0.f, 100.f, 0.f); });

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.0f, 0.f, 1.f, 1.f);

		mScene->push(model);

	}
	if (0)
	{
		Mesh mesh = createMesh(0);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition += glm::vec3(-5.f, 1.5f, 5.f); p.mPosition *= 60.f; });
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 1.f, 0.f, 1.f);

		mScene->push(model);

	}

	if (0)
	{
		// 鏡
		Mesh mesh = createMesh(2);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition *= 150.f; });

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 0.f, 0.f, 1.f);
		material[0].m_diffuse = 0.f;
		material[0].m_specular = 1.f;


		mScene->push(model);

	}
	if (0)
	{
		// 鏡
		Mesh mesh = createMesh(2);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition *= 150.f; });

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 0.f, 0.f, 1.f);
		material[0].m_diffuse = 0.f;
		material[0].m_specular = 1.f;


		mScene->push(model);

	}


	// 箱
//	if (0)
	{
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
		std::for_each(v.begin(), v.end(), [](vec3& p) {p += 0.51f; p *= vec3(90.f); });

		Mesh mesh;

		mesh.mVertex.resize(v.size() - 6);
		for (unsigned i = 0; i < v.size() - 6; i++)
		{
			mesh.mVertex[i].mPosition = v[i];
			mesh.mVertex[i].mNormal = n[i];
			mesh.mVertex[i].mMaterialIndex = i / 6;
		}

		std::vector<glm::ivec3> e((v.size() - 6) / 3);
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

		mScene->push(model);

	}

	mScene->setup();
	return mScene;
}

void Scene::push(const Model& model)
{
	mModelList.mData.push_back(model);
}
void Scene::pushLight(const Model& model)
{
	mLightList.mData.push_back(model);
}

void Scene::setup()
{
	mBuffer.setup(mModelList);
	mBuffer.setupLight(mLightList);
}

void PhotonMap::setupScene()
{

	mScene = SetupScene();
}


}
