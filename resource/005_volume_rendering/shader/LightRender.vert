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

layout(push_constant) uniform UpdateConstantBlock
{
	vec4 m_position;
	mat4 m_PV;
} constant;


void main()
{
	gl_Position = constant.m_PV * vec4(inPosition.xyz + constant.m_position, 1.0);
}
