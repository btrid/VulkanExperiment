#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#define USE_MODEL_INFO_SET 0
#include <applib/model/MultiModel.glsl>

void main()
{
	FragColor.rgb = getColor();
	FragColor.a = 1.;
}