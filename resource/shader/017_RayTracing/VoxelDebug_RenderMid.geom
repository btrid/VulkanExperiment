#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"

layout(points, invocations = 32) in;
layout(points, max_vertices = 2*64) out;


layout(location=1) in Vertex
{
	flat int VertexIndex;
}in_param[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};
layout(location=1) out Out
{
	vec3 Color;
}gs_out;

void main() 
{
	int vi = in_param[0].VertexIndex;
	if(b_hashmap[vi] < 0) { return; }

	InteriorNode top = b_interior[b_hashmap[vi]];
	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;

	ivec3 reso = u_info.reso.xyz>>4;
	int x = vi % reso.x;
	int y = (vi / reso.x) % reso.y;
	int z = (vi / (reso.x * reso.y)) % reso.z;
	vec3 p = vec3(x, y, z)*16;

	for(int g = 0; g < 2; g++)
	{
		int gi = gl_InvocationID*2+g;
		if(!isBitOn(top.bitmask, gi)) { continue;}
		vec3 lp = vec3(gi%4, (gi/4)%4, (gi/16)%4);
		lp *= 4;

		uint offset = bitcount(top.bitmask, gi);
		InteriorNode node = b_interior[top.child + offset];
		for(int m = 0; m < 64; m++)
		{			
			if((node.bitmask[m/32] & (1<<(m%32))) == 0) { continue;}
			vec3 cp = vec3(m%4, (m/4)%4, (m/16)%4);
			cp *= 1;

			{
				gl_Position = pv * vec4(p+lp+cp, 1.);
				gl_PointSize = 10. / gl_Position.w;
				gs_out.Color = vec3(1.);
				EmitVertex();
			}
			
		}
	}
	EndPrimitive();

}
