#version 450
#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)


layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


layout(location=0) in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) in vec3 Vertex[];

layout(location=1) out GSOUT
{
	vec4 Color;
}GSOut;

void main() 
{

	vec3 normal = normalize(cross(normalize(Vertex[1] - Vertex[0]), normalize(Vertex[2] - Vertex[0])));
	GSOut.Color = normal.y >= 0.5 ? vec4(0.3, 0.5, 0.9, 1.) : vec4(0.5, 0.5, 0.5, 1.);
	for(int i = 0; i < 3; i++)
	{
		gl_Position = gl_in[i].gl_Position;
		GSOut.Color.b = Vertex[i].y;
		EmitVertex();
	}	
	EndPrimitive();
}
