#pragma once
#include <ogl/oglProgram.h>
#include <vecmath.h>

#include <Texture.h>
#include <Font.h>
class DrawHelper
{
	Program program_;
	Program programTex2DArray_;
	Program textureBufferProgram_;
	unsigned vao2DHandle_;
	unsigned faceHanfle_;

	Program programSphere_;
	Program programBox_;
	Program programTexture_;
	unsigned vaoBoxHandle_;
	unsigned faceBoxHandle_;

	unsigned vaoSphereHandle_;
	unsigned faceSphereHandle_;

	unsigned vaoLineHandle_;
	unsigned vboLineHandle_;
	unsigned faceLineHandle_;

	Program meshProgram_;
//	Mesh<Vertex, unsigned> mesh_;
	Font font_;
public:

	struct DrawParam
	{
		glm::quat rot;
		glm::vec4 color;
		DrawParam(const glm::quat r = glm::quat(1.f, 0.f, 0.f, 0.f), const glm::vec4 col = glm::vec4(1.f, 0.f, 0.f, 1.f))
			: rot(r)
			, color(col)
		{

		}
	};
	static DrawHelper* instance_;
	static DrawHelper* Order();
	static void Initialize();
	static void Finalize();
	DrawHelper(void);
	~DrawHelper(void);

	void drawPlane(const glm::vec3& tl, const glm::vec3& tr, const glm::vec3& bl, const glm::vec3& br);
	void draw2DRect();
	void draw2DPoint();
	void draw2DPoint(float x, float y, float z);
	void draw2DRect(float x, float y, float width, float height, float z);
	void draw2DLine(float x0, float y0, float x1, float y1, float width, float z);
	void draw3DLine(float x0, float y0, float z0, float x1, float y1, float z1, float width, const DrawParam& param = DrawParam());
	void draw3DLine(const glm::vec3& pos0, const glm::vec3& pos1, float width, const DrawParam& param = DrawParam());
	void draw2DRect(float x, float y, float width, float height, float z, int color);
	void draw3DBox(float x, float y, float z, float width, float height, float depth, const glm::quat& rotate = glm::quat(1.f, 0.f, 0.f, 0.f));
	void draw3DSphere(float x, float y, float z, float r, const glm::quat& rotate = glm::quat(), const glm::vec4& color = glm::vec4(1.f, 0.f, 0.f, 1.f));
	void draw3DSphere(const glm::vec3& pos, float r, const DrawParam& param = DrawParam());
	void drawTexture(unsigned texHandle, float scale = 1.f);
	void drawTextureArray(unsigned texHandle);
	void drawShadow(unsigned texHandle, float scale = 1.f);
	void drawVertex(const std::vector<glm::vec4>& vertex);
	VAOHandle getSphereVAO(){ return vaoSphereHandle_; }
	unsigned getSphereCount(){ return 84; }
	VAOHandle getBoxVAO(){ return vaoBoxHandle_; }
	VAOHandle getPlaneVAO(){ return vao2DHandle_; }

	void printf(int x, int y, const wchar_t *format, ...);
	static void GetBox(std::vector<float>& vertex, std::vector<unsigned>& element);
	static void GetPlane(std::vector<glm::vec3>& vertex, std::vector<unsigned>& element);
	static void GetBox(std::vector<glm::vec3>& vertex);
	static void GetBox(std::vector<glm::vec3>& vertex, std::vector<unsigned>& element);
	static void GetSphere(std::vector<glm::vec3>& vertex, std::vector<unsigned>& element, int quarity = 0);
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::ivec3>> GetPlane();
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::ivec3>> GetBox();
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::ivec3>> GetSphere(int quarity = 0);
	static std::vector<glm::vec3> CalcNormal(const std::vector<glm::vec3>& vertex);
	static std::vector<glm::vec3> CalcNormal(const std::vector<glm::vec3>& vertex, const std::vector<unsigned>& element);
	static std::vector<glm::vec3> CalcNormal(const std::vector<glm::vec3>& vertex, const std::vector<glm::ivec3>& element);
//	static std::array<unsigned, 2> CreateVBO();
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::vec3>> createOrthoNormalBasis(const std::vector<glm::vec3>& normal) {
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

};

