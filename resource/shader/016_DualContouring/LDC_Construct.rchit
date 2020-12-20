#version 460

#extension GL_GOOGLE_include_directive : require

#define USE_AS 0
#define USE_LDC 1
#include "LDC.glsl"

struct Paylpad
{
	float HitT;
	uint PrimitiveID;
};
layout(location = 0) rayPayloadInEXT Paylpad payload;

//hitAttributeEXT vec2 baryCoord;
void main()
{
	payload.HitT = gl_HitTEXT;
	payload.PrimitiveID = gl_PrimitiveID;
}
