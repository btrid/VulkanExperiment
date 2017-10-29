#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/ConvertDimension.glsl>

#define USE_VOXEL 0
#include <btrlib/Voxelize/Voxelize.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out Vertex
{
	vec3 albedo;
}Out;

vec3 box[] = { 
	vec3(1., 1., 1.),
	vec3(-1., 1., 1.),
	vec3(1., 1., -1.),
	vec3(-1., 1., -1.),
	vec3(1., -1., 1.),
	vec3(-1., -1., 1.),
	vec3(-1., -1., -1.),
	vec3(1., -1., -1.)
};
uint elements[] = {
	3, 2, 6, 7, 4, 2, 0,
	3, 1, 6, 5, 4, 1, 0
};
void main()
{
	uvec3 index = convert1DTo3D(gl_InstanceIndex, u_voxel_info.u_cell_num.xyz);
	vec4 value = textureLod(t_voxel_sampler, u_voxel_info.u_cell_num.xyz/vec3(index), 0);
	if(dot(value.xyz, value.xyz) >= 0.001)
	{
		vec3 size = u_voxel_info.u_cell_size.xyz;
		Out.albedo = value.xyz;
		vec3 vertex = box[elements[gl_VertexIndex]]*0.3;
		gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4((vertex+vec3(index))*size+u_voxel_info.u_area_min.xyz, 1.0);
	}
	else
	{
		Out.albedo = vec3(0.);
		gl_Position = vec4(-1., -1., 1., 1.);
	}

}
