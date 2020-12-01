#version 460

#extension GL_GOOGLE_include_directive : require
#include "LDC.glsl"

layout(location = 0) rayPayloadInEXT float RayMaxT;


void main()
{
  RayMaxT = -1.;
}