#version 450

//#extension GL_ARB_shader_draw_parameters : require

#extension GL_GOOGLE_cpp_style_line_directive : require

out gl_PerVertex{
	vec4 gl_Position;
};

struct Vertex
{
	vec2 Texcoord;
};
layout(location = 0) out Vertex VSOut;

void main()
{
	float x = float((gl_VertexIndex & 1) << 2);
    float y = float((gl_VertexIndex & 2) << 1);
	VSOut.Texcoord = vec2(x, y);
	gl_Position = vec4(vec2(x, y)*2.-1., 0, 1);
 }
