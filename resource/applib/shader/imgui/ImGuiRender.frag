#version 450
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

layout (set=0, binding = 0) uniform sampler2D tFont;


layout(location=0) out vec4 FragColor;
layout(location = 1) in PerVertex{
	vec2 texcoord;
	uint color;
}vertex;

void main()
{
//	uint a = (vertex.color/255/255/255)%255;
//	uint b = (vertex.color/255/255)%255;
//	uint g = (vertex.color/255)%255;
//	uint r = (vertex.color)%255;
	uint r = (vertex.color/255/255/255)%255;
	uint g = (vertex.color/255/255)%255;
	uint b = (vertex.color/255)%255;
	uint a = (vertex.color)%255;
	FragColor = vec4(r, g, b, a) / 255.;
	FragColor.a = texture(tFont, vertex.texcoord).r;
}