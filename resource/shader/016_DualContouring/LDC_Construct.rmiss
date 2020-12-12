#version 460

#extension GL_GOOGLE_include_directive : require
#include "LDC.glsl"

layout(location = 0) rayPayloadInEXT float RayMaxT;
layout(location = 1) rayPayloadInEXT uint PrimitiveID;


void main()
{
  RayMaxT = -10.;
}