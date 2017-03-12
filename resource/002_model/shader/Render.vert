#version 330

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

struct Camera
{
	mat4 mProjection;
	mat4 mView;	
};
layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec4 inColor;

out gl_PerVertex{
	vec4 gl_Position;
};

out VSOut{
	vec4 color;
}Out;

layout(std140, binding=0) uniform CameraUniform
{
	Camera uCamera;
};
void main()
{
	vec4 pos = vec4(inPosition.xyz, 1.0);
	gl_Position = uCamera.mProjection * uCamera.mView * pos;
	Out.color = inColor;
}
