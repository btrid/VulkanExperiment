#version 450

layout (location = 0) out vec2 outUV;

void main()
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2)*2.-1.;
	gl_Position = vec4(outUV, 0.0f, 1.0f);
}