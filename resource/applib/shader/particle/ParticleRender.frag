#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

//layout(early_fragment_tests) in;

layout(location=0) out vec4 FragColor;

layout(location=1) in FSIN{
	vec4 m_color;
}FSIn;

void main()
{
	FragColor.rgba = FSIn.m_color;
//	FragColor.a = 1.;
}