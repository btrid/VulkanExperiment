#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_Voxel 0
#define USE_Model 1
#include "Voxel.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=1)in InVertex{
	vec3 Position;
	vec3 Albedo;
}In[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out Transform{
	vec3 Position;
	vec3 Albedo;
	vec3 Normal;
}transform;

void main() 
{
//	for(int i = 0; i<99999999; i++){ i = 0;}

	vec3 n = normalize(cross(In[1].Position-In[0].Position, In[2].Position-In[0].Position));
	float x = dot(n, vec3(1., 0., 0.));
	float y = dot(n, vec3(0., 1., 0.));
	float z = dot(n, vec3(0., 0., 1.));

	mat4 pv = u_voxelize_pvmat[0];
	for(int i = 0; i < 3; i++)
	{
		gl_Position = vec4(In[i].Position, 1.);
		if(abs(x) > abs(y) && abs(x) > abs(z))
		{
			gl_Position.xyz = gl_Position.zyx;
		}
		else if(abs(y) > abs(z))
		{		
			gl_Position.xyz = gl_Position.xzy;
		}
		else
		{
		}

		gl_Position = pv * gl_Position;
		transform.Position = In[i].Position;
		transform.Albedo = In[i].Albedo;
		transform.Normal = n;
		EmitVertex();
	}	
	EndPrimitive();

}
