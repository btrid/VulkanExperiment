#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_Model 0
#include "CSG.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"
layout(location=1) in Transform
{
	vec3 normal;
	flat uvec3 CellID;
}In;


layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 normal = normalize(In.normal);
	FragColor = vec4(normal, 1.);

}