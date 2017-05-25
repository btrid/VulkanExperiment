#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require


#include </ConvertDimension.glsl>
#include </Shape.glsl>
#include </TriangleLL.glsl>
#include </PM.glsl>


layout(location = 0)in vec3 inPosition;

out gl_PerVertex
{
	vec4 gl_Position;
};

out OutVertex{
	flat int Visible;
}Out;

layout(std140, set=0, binding=0) uniform BrickParamUniform {
	BrickParam uParam;
};

layout(std430, set=0, binding = 1) readonly restrict buffer TriangleLLHeadBuffer{
	uint bTriangleLLHead[];
};


layout(push_constant) uniform UpdateConstantBlock
{
	mat4 PV;
} constant;

void main()
{
	BrickParam param = uParam;
	if(gl_InstanceIndex < param.m_total_num.w)
	{
		uvec3 index = convert1DTo3D(gl_InstanceIndex, param.m_total_num.xyz);
		uint i = bTriangleLLHead[getTiledIndexBrick1(param, index)];
		Out.Visible = ((i != 0xFFFFFFFF) ? 1 : 0);

		vec3 size = getCellSize1(param);
		gl_Position = constant.PV * vec4((inPosition+vec3(index))*size+param.m_area_min.xyz, 1.0);

	}else{
		Out.Visible = 1;
		gl_Position = constant.PV * vec4(inPosition*(param.m_area_max-param.m_area_min).xyz, 1.0);
	}

}
