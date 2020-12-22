#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LDC 0
#define USE_Rendering 2
#include "LDC.glsl"

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
	Out.VertexIndex = gl_VertexIndex;
	gl_Position = vec4(1.);

}
