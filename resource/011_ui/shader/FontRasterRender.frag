#version 450
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#define USE_FONT_TEXTURE 1
#include <Font.glsl>


layout(location=0) out vec4 FragColor;
layout(location = 1) in VS{
	vec2 texcoord;
}fsin;

void main()
{
	vec4 color = texture(tGlyph, fsin.texcoord).rrrr;
	FragColor = vec4(1., 1., 1., color.r);
}