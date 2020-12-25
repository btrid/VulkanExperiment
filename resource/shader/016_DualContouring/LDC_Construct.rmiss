#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Model 0
#define USE_DC 1
#include "DualContouring.glsl"

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