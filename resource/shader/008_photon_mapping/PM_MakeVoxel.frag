#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_PM 0
#include "PM.glsl"


in Transform{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

layout(location = 0) out vec4 FragColor;

void main()
{
	
	ivec3 indexMap0 = getIndexBrick0(uParam, transform.Position.xyz);
	if(!isInRange(indexMap0, uParam.num0))
	{ return; }
	uint index = imageAtomicCompSwap(tBrickMap0, indexMap0, 0xFFFFFFFF, 0);
	if(index != 0xFFFFFFFF)
	{ return; }
	
	uint newIndex = atomicCounterIncrement(aBrick0Count);
	imageStore(tBrickMap0, indexMap0, ivec4(newIndex));
}