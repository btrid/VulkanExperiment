#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)

#extension GL_ARB_shader_draw_parameters : require
//#extension GL_ARB_bindless_texture : require


#ifdef VULKAN
#extension GL_GOOGLE_cpp_style_line_directive : require
//#extension GL_KHR_vulkan_glsl : require
#else
#extension GL_ARB_shading_language_include : require
#endif
#include </Math.glsl>
#define USE_MODEL_INFO_SET 0
//#define USE_MODEL_INFO_SET 0
#define USE_SCENE_SET 2
#include </MultiModel.glsl>

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in vec4 inTexcoord;
layout(location = 3)in uvec4 inBoneID;
layout(location = 4)in vec4 inWeight;


out gl_PerVertex{
	vec4 gl_Position;
};

struct Vertex
{
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
};
layout(location = 0) out Vertex VSOut;

layout(std430, set=0, binding=0) restrict buffer BoneTransformBuffer {
	mat4 bones[];
};
layout(std430, set=0, binding=1) restrict buffer MaterialBuffer {
	Material b_material[];
};

layout(std140, set=2, binding=0) uniform CameraUniform {
	Camera2 uCamera;
};
layout(push_constant) uniform UpdateConstantBlock
{
	mat4 m_world;
} constant;

mat4 skinning()
{
	mat4 transMat = mat4(0.0);
	for(int i=0; i<4; i++)
	{
		if(inBoneID[i] != 255) 
		{
			transMat += inWeight[i] * bones[inBoneID[i]];
		}
	}
	return transMat;
}

void main()
{
	vec4 pos = vec4((inPosition).xyz, 1.0);
	mat4 skinningMat = skinning();
	pos = skinningMat * constant.m_world * pos;
	Camera2 camera = uCamera;
	gl_Position = camera.uProjection * camera.uView * vec4(pos.xyz, 1.0);

	VSOut.Position = pos.xyz;
	VSOut.Normal = /*mat3(skinningMat) **/ /*mat3(transpose(inverse(uView))) **/ inNormal.xyz;
	VSOut.Texcoord = inTexcoord.xyz;
}
