#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

layout (set=0, binding=0) uniform sampler2D tSrc;
layout (set=0, binding=1) uniform sampler2D tDst;
//layout (set=0, binding=1, rgba) uniform texture2D tDst;

struct Vertex
{
	vec2 Texcoord;
};
layout(location=0) in Vertex VSIn;
layout(location=0) out vec4 FragColor;

void main()
{
	// ガンマ補正
	vec4 color = texture(tSrc, VSIn.Texcoord);
//	FragColor = pow(color, vec4(2.2,2.2,2.2, 1.));
	FragColor = color;
}
