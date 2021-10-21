#version 450

#extension GL_GOOGLE_include_directive : require
//#extension GL_KHR_vulkan_glsl : require

layout(push_constant) uniform UpdateConstantBlock
{
	uint face;
} constant;

layout(set=0, binding=0) uniform samplerCube tex;

layout (location = 0) in vec2 inUV;
layout(location = 0) out vec4 FragColor;

vec4 cubemapping()
{
	vec3 uvw = vec3(inUV.x, inUV.y, 0.);
	switch(constant.face)
	{
		case 0: uvw = vec3(1, inUV.y, -inUV.x); break;
		case 1: uvw = vec3(-1, inUV.y, inUV.x); break;
		case 2: uvw = vec3(inUV.x, -1, inUV.y); break;
		case 3: uvw = vec3(inUV.x, 1., -inUV.y); break;
		case 4: uvw = vec3(inUV.x, inUV.y, 1.); break;
		case 5: uvw = vec3(-inUV.x, inUV.y, -1.); break;
	}
	return texture(tex, uvw);
}
void main()
{	
	FragColor = cubemapping();

}