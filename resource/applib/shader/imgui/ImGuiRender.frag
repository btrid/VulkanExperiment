#version 450
#extension GL_GOOGLE_include_directive : require

layout (set=0, binding = 0) uniform sampler2D tFont;

layout(location = 1) in PerVertex{
	noperspective  vec2 texcoord;
	noperspective  vec4 color;
}vertex;

layout(location=0) out vec4 FragColor;

void main()
{
	FragColor = vertex.color * texture(tFont, vertex.texcoord).r;
//	FragColor = vec4(1.);
}