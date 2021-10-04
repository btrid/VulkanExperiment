#version 460
#extension GL_GOOGLE_include_directive : require


#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

#define USE_Render_Scene 1
#include "pbr.glsl"


layout(location = 1) in A
{
	vec3 Texcoord_0;
}In;

layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 t = normalize(In.Texcoord_0);
	FragColor = texture(t_environment, t);
	switch(u_render_config.skybox_render_type)
	{
		case 1:
		{
			float mip = textureQueryLevels(t_environment_irradiance);
			FragColor = textureLod(t_environment_irradiance, t, mip*u_render_config.lod);
		}
		break;
		case 2:
		{
			float mip = textureQueryLevels(t_environment_prefiltered);
			FragColor = textureLod(t_environment_prefiltered, t, mip*u_render_config.lod);
		}
		break;
	}
//	FragColor.xyz = normalize(In.Texcoord_0);
	FragColor.w = 1.;
	FragColor = tonemap(FragColor);
}