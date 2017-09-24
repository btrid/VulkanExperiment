#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require

#include </convertDimension.glsl>

#define SETPOINT_VOXEL 0
#include </Voxelize.glsl>

#define SETPOINT_CAMERA 1
#include </Camera.glsl>

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
	uvec3 index = convert1DTo3D(gl_InstanceIndex, uvec3(32, 32, 32));
	uint packd_value = imageLoad(t_voxel_albedo, ivec3(index)).r;
	uint count = (packd_value)&((1<<5)-1);
	if(count != 0)
	{
		uint packd_albedo_r = (packd_value>>23)&((1<<9)-1);
		uint packd_albedo_g = (packd_value>>14)&((1<<9)-1);
		uint packd_albedo_b = (packd_value>>5)&((1<<9)-1);
		vec3 albedo = vec3(packd_albedo_r, packd_albedo_g, packd_albedo_b) / 64. / float(count);
		vec3 size = u_voxel_info.u_cell_size.xyz;
		Out.albedo = vec3(albedo / float(count));
		vec3 vertex = box[elements[gl_VertexIndex]]*0.3;
		gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4((vertex+vec3(index))*size+u_voxel_info.u_area_min.xyz, 1.0);
	}
	else
	{
		Out.albedo = vec3(0.);
		gl_Position = vec4(-1., -1., 1., 1.);
	}

}
