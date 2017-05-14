#version 450
#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)


layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

in GeometryIN
{
	vec3 Vertex;
}In[];

out GSOUT
{
	vec4 Color;
}GSOut;

void main() 
{

	vec3 normal = normalize(cross(normalize(In[1].Vertex - In[0].Vertex), normalize(In[2].Vertex - In[0].Vertex)));
	GSOut.Color = normal.y >= 0.5 ? vec4(0.3, 0.5, 0.9, 1.) : vec4(0.5, 0.5, 0.5, 1.);
	for(int i = 0; i < 3; i++)
	{
		gl_Position = gl_in[i].gl_Position;
		GSOut.Color.b = In[i].Vertex.y;
		EmitVertex();
	}	
	EndPrimitive();
}
