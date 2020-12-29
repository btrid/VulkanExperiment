#version 460

#extension GL_GOOGLE_include_directive : require

#define USE_Model 0
#define USE_DC 1
#include "DualContouring.glsl"

struct Paylpad
{
	float HitT;
	uint PrimitiveID;
	uint HitKind;
};
layout(location = 0) rayPayloadInEXT Paylpad payload;

//hitAttributeEXT vec2 baryCoord;
void main()
{
	payload.HitT = gl_HitTEXT;
	payload.PrimitiveID = gl_PrimitiveID;
	payload.HitKind = gl_HitKindEXT==gl_HitKindFrontFacingTriangleEXT?1:0;
}
