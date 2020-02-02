#version 460
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"

#define USE_VOXEL 0
#include "Voxelize.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out Vertex
{
	vec3 albedo;
}Out;

vec3 box[] = { 
	vec3(-1., 1., 1.),  
	vec3(1., -1., 1.),
	vec3(1., 1., 1.),
	vec3(1., 1., -1.),
	vec3(-1., 1., 1.),
	vec3(-1., 1., -1.),
	vec3(-1., -1., -1.),
	vec3(1., 1., -1.),
	vec3(1., -1., -1.), 
	vec3(1., -1., 1.),  
	vec3(-1., -1., -1.), 
	vec3(-1., -1., 1.),
	vec3(-1., 1., 1.),  
	vec3(1., -1., 1.),
};

layout(push_constant) uniform ConstantBlock
{
	int m_miplevel;
} constant;

void main()
{
	int mipmap = constant.m_miplevel;
	uvec3 cell_num = u_voxel_info.u_cell_num.xyz/(1<<mipmap);
	cell_num = max(cell_num, uvec3(1));

	uvec3 index = convert1DTo3D(gl_InstanceIndex, cell_num);
	vec4 value = texelFetch(t_voxel_sampler, ivec3(index), mipmap);
	if(dot(value.xyz, value.xyz) >= 0.0003)
	{
		vec3 size = u_voxel_info.u_cell_size.xyz*(1<<mipmap);
		Out.albedo = vec3(1.);
		vec3 vertex = box[gl_VertexIndex]*0.3;
		gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4((vertex+vec3(index))*size + size*0.5 + u_voxel_info.u_area_min.xyz, 1.0);
	}
	else
	{
		Out.albedo = vec3(0.);
		gl_Position = vec4(-1., -1., 1., 1.);
	}

}
