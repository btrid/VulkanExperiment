#version 450

//#extension GL_ARB_shader_draw_parameters : require

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/Math.glsl>

#define USE_MODEL_INFO_SET 0
#include <applib/model/MultiModel.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

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

mat4 skinning()
{
	mat4 transMat = mat4(0.0);
	for(int i=0; i<4; i++)
	{
		if(inBoneID[i] != 255) 
		{
			transMat += inWeight[i] * b_bone_transform[inBoneID[i]];
		}
	}
	return transMat;
}

void main()
{
	vec4 pos = vec4((inPosition).xyz, 1.0);
	mat4 skinningMat = skinning();
	pos = skinningMat * pos;
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4(pos.xyz, 1.0);

	VSOut.Position = pos.xyz;
	VSOut.Normal = mat3(skinningMat) * inNormal.xyz;
	VSOut.Texcoord = inTexcoord.xyz;
}
