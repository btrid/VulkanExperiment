#version 460
#extension GL_GOOGLE_include_directive : require

//#define USE_AppModel 0
//#define USE_AppModel_Render 1
//#include "applib/AppModel.glsl"

#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

#define USE_Render_Scene 1
#include "pbr.glsl"

#define USE_Model_Resource 2
#include "Model.glsl"

#define USE_Model_Entity 3
layout(set=USE_Model_Entity, binding=0, scalar) buffer ModelEntityBuffer { Entity b_model_entity[]; };

layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location = 1) out Vertex
{
	vec3 WorldPos;
	vec3 Normal;
	vec2 Texcoord_0;
}Out;

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
	Entity entity = b_model_entity[0];
	I Index  = I(entity.Index);
	V vertex  = Index.b_i[gl_VertexIndex];
	vertex[v];

	vec4 pos = vec4(inPosition.xyz, 1.0);
//	pos.y = -pos.y;
//	mat4 skinningMat = skinning();
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * pos;

	Out.WorldPos.xyz = pos.xyz/pos.w;
	Out.Normal.xyz =  inNormal.xyz;
	Out.Texcoord_0 = inTexcoord_0;
}
