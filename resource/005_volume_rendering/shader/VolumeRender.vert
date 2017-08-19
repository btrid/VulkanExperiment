#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
//#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_GOOGLE_cpp_style_line_directive : require

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out VSOut
{
	vec2 texcoord;
}Out;

void main()
{
	vec2 pos = vec2(gl_VertexIndex/2, gl_VertexIndex%2) * 2. - 1.;
	gl_Position = vec4(pos, 0., 1.);
	Out.texcoord = gl_Position.xy;
}
