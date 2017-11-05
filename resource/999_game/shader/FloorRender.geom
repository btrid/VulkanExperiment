#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/ConvertDimension.glsl>

#define SETPOINT_CAMERA 0
#include <btrlib/Camera.glsl>

#define SETPOINT_MAP 1
#define SETPOINT_SCENE 2
#include </Map.glsl>


layout(points) in;
layout(triangle_strip, max_vertices = 14) out;

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=0)in Vertex
{
	uint instanceIndex;
}In[];

layout(location=1) out GSOUT
{
	uint map;
	vec3 axis;
}GSOut;


vec3 box[] = { 
	vec3(0., 1., 1.),  
	vec3(1., 0., 1.),
	vec3(1., 1., 1.),
	vec3(1., 1., 0.),
	vec3(0., 1., 1.),
	vec3(0., 1., 0.),
	vec3(0., 0., 0.),
	vec3(1., 1., 0.),
	vec3(1., 0., 0.), 
	vec3(1., 0., 1.),  
	vec3(0., 0., 0.), 
	vec3(0., 0., 1.),
	vec3(0., 1., 1.),  
	vec3(1., 0., 1.),
};
void main() 
{
	MapDescriptor desc = u_map_info.m_descriptor[0];
	ivec2 index = ivec2(In[0].instanceIndex%desc.m_cell_num.x, In[0].instanceIndex/desc.m_cell_num.y);
	uint map = imageLoad(t_map, ivec2(index)).x;

	vec3 offset = vec3(desc.m_cell_size.x*index.x, 0., desc.m_cell_size.y*index.y);
	vec3 size = vec3(desc.m_cell_size.x, map*5., desc.m_cell_size.y);
	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;


	vec4 p1 = pv * vec4(offset + size, 1.);
	p1 /= p1.w;
	if(all(greaterThan(p1.xy, vec2(1.))))
	{
		// camera culling
		return;
	}
	vec4 p2 = pv * vec4(offset + vec3(-size.x, 0., -size.z), 1.);
	p2 /= p2.w;
	if(all(lessThan(p2.xy, vec2(-1.))))
	{
		// camera culling
		return;
	}

	GSOut.map = map;
	for(int i = 0; i < 14; i++)
	{
		GSOut.axis = box[i];
		vec3 vertex = box[i] * size;
		gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4(offset + vertex, 1.);

		EmitVertex();
	}	
	EndPrimitive();
}
