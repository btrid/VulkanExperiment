#version 460

#extension GL_GOOGLE_include_directive : require

#define USE_AS 0
#define USE_LDC 1
#include "LDC.glsl"

layout(location = 0) rayPayloadInEXT float RayMaxT;
layout(location = 1) rayPayloadInEXT uint PrimitiveID;

hitAttributeEXT vec2 baryCoord;
void main()
{
  RayMaxT = gl_RayTmaxEXT;
  PrimitiveID = gl_PrimitiveID;


}
