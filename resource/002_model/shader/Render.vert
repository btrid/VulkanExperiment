#version 450

#extension GL_ARB_shader_draw_parameters : require
//#extension GL_ARB_bindless_texture : require


#ifdef VULKAN
#extension GL_GOOGLE_cpp_style_line_directive : require
//#extension GL_KHR_vulkan_glsl : require
#else
#extension GL_ARB_shading_language_include : require
#endif
#include </Math.glsl>
#include </MultiModel.glsl>

layout(location = 0)in vec4 inPosition;
layout(location = 1)in vec4 inNormal;
layout(location = 2)in vec4 inTexcoord;
layout(location = 3)in ivec4 inBoneID;
layout(location = 4)in vec4 inWeight;
//layout(location = 5)in int inMaterialIndex;


out gl_PerVertex{
	vec4 gl_Position;
};

struct Vertex
{
//	flat int MaterialIndex;
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
};
layout(location = 0) out Vertex VSOut;

layout(std140, set=2, binding=0) uniform CameraUniform
{
	mat4 uProjection;
	mat4 uView;
};

layout(std140, set=0, binding=0) uniform ModelInfoUniform
{
	ModelInfo modelInfo;
};

struct VSMaterial {
	uvec2	NormalTex;
	uvec2	HeightTex;
};

layout(std430, set=0, binding=1) readonly buffer VSMaterialBuffer
{
	VSMaterial vsMaterial[];
};

layout(std430, set=0, binding=2)readonly restrict buffer BoneTransformBuffer {
	mat4 bones[];
};


mat4 skinning()
{
	mat4 transMat = mat4(0.0);
	float w = 0.;
	for(int i=0; i<4; i++)
	{
		if(inBoneID[i] >= 0) 
		{
			transMat += inWeight[i] * bones[modelInfo.boneNum * int(gl_InstanceIndex) + inBoneID[i]];
			w += inWeight[i];
		}
	}
	
	if(w <= 0.99 || w >= 1.01)
	{
		transMat = mat4(1.0);
	}
	
	return transMat;
}

void main()
{
	VSOut.Texcoord = inTexcoord.xyz;
//	VSMaterial m = vsMaterial[inMaterialIndex];

	vec4 offset = vec4(0.);
//	if(m.HeightTex != 0) {
//		offset = texture(sampler2D(m.HeightTex), VSOut.Texcoord.xy)*2.;
//	}
	mat4 skin = skinning();
	mat4 scale = mat4(1.);
	vec4 pos = skin * vec4(vec4(inPosition + offset).xyz, 1.0);
//	pos.xyz *= 5000.;
//	pos.xyz += vec3(1.) * gl_InstanceIndex ;
	gl_Position = uProjection * uView * pos;

	VSOut.Normal = /*m.NormalTex != 0 ? texture(sampler2D(m.NormalTex), VSOut.Texcoord.xy).xyz : */inNormal.xyz;

//	VSOut.MaterialIndex = inMaterialIndex;
}
