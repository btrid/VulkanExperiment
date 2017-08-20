#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
//#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_GOOGLE_cpp_style_line_directive : require

layout(location = 0) out vec4 FragColor;

void main()
{	
	FragColor = vec4(1.);
}