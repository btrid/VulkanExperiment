#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require


#include </ConvertDimension.glsl>
#include </Shape.glsl>
#include </Brick.glsl>
#include </PM.glsl>


layout(location = 0)in vec3 inPosition;

out gl_PerVertex
{
	vec4 gl_Position;
};

out OutVertex{
	flat int Visible;
}Out;

uniform mat4 PV = mat4(1.);

layout(std140, binding=0) uniform BrickParamUniform {
	BrickParam uParam;
};

layout(std430, binding = 1) readonly restrict buffer TriangleLLHeadBuffer{
	uint bTriangleLLHead[];
};


layout(push_constant) uniform UpdateConstantBlock
{
	mat4 PV;
} constant;

void main()
{
	if(gl_InstanceID < uParam.m_total_num.w)
	{
		ivec3 index = convert1DTo3D(gl_InstanceID, uParam.m_total_num.xyz);
		uint i = bTriangleLLHead[getTiledIndexBrick1(uParam, index)];
		Out.Visible = ((i != 0xFFFFFFFF) ? 1 : 0);

//		if(Out.Visible != 0)
		{
			vec3 size = getCellSize1(uParam);
			gl_Position = constant.PV * vec4((inPosition+vec3(index))*size+uParam.m_area_min.xyz, 1.0);
//		}else{
//			gl_Position = vec4(-11., -11., 0., 0.);
		}
	}else{
		Out.Visible = 1;
		gl_Position = constant.PV * vec4(inPosition*(uParam.m_area_max-uParam.m_area_min).xyz, 1.0);
	}

}
