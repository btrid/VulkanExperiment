#version 460
#extension GL_GOOGLE_include_directive : require

layout(set=0, binding=0, std140) uniform _Dummy
{
	int a;
};
#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

#define USE_RenderTarget 2
#include "applib/System.glsl"

layout(location = 0) out vec4 FragColor;
void main()
{
	FragColor = vec4(1,1,1,1);

}