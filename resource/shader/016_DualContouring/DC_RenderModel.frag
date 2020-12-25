#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_DC 0
#define USE_Rendering 2
#include "DualContouring.glsl"

layout(location=1) in Transform{
	flat uvec3 CellID;
}transform;


layout(location = 0) out vec4 FragColor;
void main()
{
	FragColor = vec4(1.);

}