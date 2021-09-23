#version 460
#extension GL_GOOGLE_include_directive : require
// https://substance3d.adobe.com/tutorials/courses/the-pbr-guide-part-1
// https://substance3d.adobe.com/tutorials/courses/the-pbr-guide-part-2
#define M_PI 3.141592

 layout(location = 0) out vec4 FragColor;
#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

#define USE_Render_Scene 1
#include "pbr.glsl"

layout(location = 1) in A
{
	vec3 Texcoord_0;
}In;
void main()
{
	FragColor = texture(t_environment_irradiance, In.Texcoord_0);
//	FragColor.xyz = abs(In.Texcoord_0);
	FragColor.w = 1.;
	FragColor = tonemap(FragColor);
}