#version 450

#extension GL_GOOGLE_include_directive : require

layout (set=0, binding=0) uniform sampler2D tex;

layout (location = 0) in vec2 inUV;
layout(location = 0) out vec4 FragColor;

void main()
{	
	FragColor = texture(tex, inUV);
//	FragColor = vec4(texture(tex, inUV).x, 0.,0.,1.);
}