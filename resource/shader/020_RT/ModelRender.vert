#version 460
#extension GL_GOOGLE_include_directive : require

//#define USE_AppModel 0
//#define USE_AppModel_Render 1
//#include "applib/AppModel.glsl"

#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in vec2 inTexcoord_0;
//layout(location = 3)in uvec4 inBoneID;
//layout(location = 4)in vec4 inWeight;


layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location = 1) out Vertex
{
	vec3 WorldPos;
	vec3 Normal;
	vec2 Texcoord_0;
}VSOut;

/*
mat4 skinning()
{
	mat4 transMat = mat4(0.0);
	uint bone_offset = u_model_info.boneNum * gl_InstanceIndex;
	for(uint i=0; i<4; i++)
	{
		if(inBoneID[i] != 255) 
		{
			transMat += inWeight[i] * b_bone_transform[bone_offset + inBoneID[i]];
		}
	}
	return transMat;
}
*/
void main()
{
	vec4 pos = vec4(inPosition.xyz, 1.0);
//	mat4 skinningMat = skinning();
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view *  100.*pos;

	VSOut.WorldPos = gl_Position.xyz / gl_Position.w;
	VSOut.Normal =  inNormal.xyz;
	VSOut.Texcoord_0 = inTexcoord_0;
}
