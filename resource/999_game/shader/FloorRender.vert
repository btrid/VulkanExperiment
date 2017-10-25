#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/ConvertDimension.glsl>

#define SETPOINT_VOXEL 0
#include <btrlib/Voxelize/Voxelize.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

layout(location=0)out Vertex
{
	uint instanceIndex;
}Out;

void main()
{
	Out.instanceIndex = gl_InstanceIndex;

}
