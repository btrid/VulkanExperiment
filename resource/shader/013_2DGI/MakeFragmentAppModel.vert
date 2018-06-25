#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/Math.glsl"

#define USE_GI2D 0
#include "GI2D.glsl"

#define USE_APPMODEL 1
#include "applib/model/MultiModel.glsl"

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in vec4 inTexcoord;
layout(location = 3)in uvec4 inBoneID;
layout(location = 4)in vec4 inWeight;

out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out ModelData
{
	vec2 texcoord;
}out_modeldata;

mat4 skinning()
{
	mat4 transMat = mat4(0.0);
	for(int i=0; i<4; i++)
	{
		if(inBoneID[i] != 255) 
		{
			transMat += inWeight[i] * b_bone_transforms[inBoneID[i]];
		}
	}
	return transMat;
}

void main()
{
	vec4 pos = vec4((inPosition).xyz, 1.0);
	mat4 skinningMat = skinning();
	pos = u_gi2d_info.m_camera_PV * skinningMat * pos;
	gl_Position = vec4(pos.xyz, 1.0);

	out_modeldata.texcoord = inTexcoord.xy;

}
