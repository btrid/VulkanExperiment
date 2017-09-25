#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require
#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>
#include </TriangleLL.glsl>


layout(binding=16, r8ui) uniform writeonly uimage3D tBrickMap0;

layout(std140, set=0, binding = 0) uniform BrickUniform
{
	BrickParam uParam;
};
layout(std430, set=0, binding = 2) restrict buffer TriangleLLHeadBuffer{
	uint bTriangleLLHead[];
};
layout(std430, set=0, binding=3) writeonly restrict buffer TriangleLLBuffer {
	TriangleLL bTriangleLL[];
};
layout(std430, set=0, binding=4) restrict buffer TriangleCountBuffer {
	uint b_triangle_count;
};


layout(location=1) in Transform{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

//layout(location = 0) out vec4 FragColor;

void main()
{
	BrickParam param = uParam;
	uvec3 indexMap0 = getIndexBrick0(param, transform.Position);
	if(isInRange(indexMap0, uParam.m_cell_num.xyz))
	{
		imageStore(tBrickMap0, ivec3(indexMap0), uvec4(1));
		uint tIndex = atomicAdd(b_triangle_count, 1);

		uvec3 indexMap1 = getIndexBrick1(param, transform.Position.xyz);
		uint old = atomicExchange(bTriangleLLHead[getTiledIndexBrick1(param, indexMap1)], tIndex);
		TriangleLL t;
		t.next = old;
		t.drawID = transform.DrawID;
		t.instanceID = transform.InstanceID;
		t.index[0] = transform.VertexID[0];
		t.index[1] = transform.VertexID[1];
		t.index[2] = transform.VertexID[2];

		bTriangleLL[tIndex] = t;
	}
}