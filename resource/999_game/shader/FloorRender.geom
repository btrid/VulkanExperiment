#version 450
#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)


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
	vec4 Color;
}GSOut;


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
	for(int i = 0; i < 14; i++)
	{
		gl_Position = vec4(box[elements[i]], 1.);
//		GSOut.Color.b = Vertex[i].y;
		EmitVertex();
	}	
	EndPrimitive();
}
