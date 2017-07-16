
#version 450
#pragma optionNV (unroll none)
#pragma optionNV (inline all)

//#extension GL_ARB_bindless_texture : require
//#extension GL_NV_gpu_shader5 : require
//#extension GL_ARB_shading_language_include : require
//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

//layout(early_fragment_tests) in;

layout(location=0) out vec4 FragColor;


layout(location=1) in FSIN
{
	vec4 Color;
}FSIn;

void main()
{
	FragColor = FSIn.Color;
}