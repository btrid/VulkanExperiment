#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require
#include </ConvertDimension.glsl>
#include </Shape.glsl>
#include </TriangleLL.glsl>


layout(binding=16, r8ui) uniform writeonly uimage3D tBrickMap0;

layout(std140, binding = 0) uniform BrickUniform
{
	BrickParam uParam;
};
layout(std430, binding = 1) restrict buffer TriangleLLHeadBuffer{
	uint bTriangleLLHead[];
};
layout(std430, binding=2) writeonly restrict buffer TriangleLLBuffer {
	TriangleLL bTriangleLL[];
};
layout(std430, binding=3) restrict buffer TriangleCountBuffer {
	uint b_triangle_count;
};


in Transform{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

//layout(location = 0) out vec4 FragColor;

void main()
{
	BrickParam param = uParam;
	ivec3 indexMap0 = getIndexBrick0(param, transform.Position);
	if(isInRange(indexMap0, uvec3(uParam.m_cell_num)))
	{
		imageStore(tBrickMap0, indexMap0, uvec4(1));
		uint tIndex = atomicAdd(b_triangle_count, 1);

		ivec3 indexMap1 = getIndexBrick1(param, transform.Position.xyz);
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