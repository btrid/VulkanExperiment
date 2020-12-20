#version 460

#extension GL_GOOGLE_include_directive : require
#include "LDC.glsl"

struct Paylpad
{
	float HitT;
	uint PrimitiveID;
};
layout(location = 0) rayPayloadInEXT Paylpad payload;


void main()
{
  payload.HitT = -10.;
}