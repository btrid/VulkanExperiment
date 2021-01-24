#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_Boolean 0
#include "DualContouring.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"


layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out VSVertex
{
	flat uint VertexIndex;
}Out;

void main()
{
	gl_Position = vec4(0., 0., 0., 1.);
}
