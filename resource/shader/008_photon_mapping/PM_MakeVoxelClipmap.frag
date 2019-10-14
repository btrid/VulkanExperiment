#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_PM 0
#include "PM.glsl"

layout(location=1) in Transform{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

void main()
{
	
//	uvec3 index0 = getIndexBrick0(uParam, transform.Position.xyz);
//	if(!isInRange(indexMap0, uParam.num0.xyz))
//	{ return; }
//	uint index = imageAtomicCompSwap(tBrickMap0, indexMap0, 0xFFFFFFFF, 0);
//	if(index != 0xFFFFFFFF)
//	{ return; }
	
//	uint newIndex = atomicCounterIncrement(aBrick0Count);
//	imageStore(tBrickMap0, indexMap0, ivec4(newIndex));

	uvec3 bv_index3d = uvec3(transform.Position.xyz / u_pm_info.num0.xyz);
	uint bv_index1d = convert3DTo1D(bv_index3d, u_pm_info.num0.xyz);
	uint t_index = atomicAdd(b_triangle_count.x, 1);
	uint old = atomicExchange(b_triangle_LL_head[bv_index1d], t_index);

	TriangleLL t;
	t.next = old;
	t.drawID = transform.DrawID;
	t.instanceID = transform.InstanceID;
	t.index[0] = transform.VertexID[0];
	t.index[1] = transform.VertexID[1];
	t.index[2] = transform.VertexID[2];

	b_triangle_LL[t_index] = t;
}