#version 450
#pragma optionNV (unroll none)
#pragma optionNV (inline all)

//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

layout(early_fragment_tests) in;

layout(location=0) out vec4 FragColor;

layout(location=1) in FSIN{
	float life;
}FSIn;

void main()
{
	FragColor.rgb = vec3(1.);
	FragColor.a = 1.;
}