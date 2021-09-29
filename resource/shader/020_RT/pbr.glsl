#ifndef PBR_H_
#define PBR_H_

#ifdef USE_Render_Scene

struct RenderConfig
{
	float exposure;
	float gamma;
};

layout(set=USE_Render_Scene, binding=0, std140) uniform UScene 
{
	RenderConfig u_render_config;
};
layout (set=USE_Render_Scene, binding=10) uniform samplerCube t_environment_irradiance;



vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

vec4 tonemap(vec4 color)
{
	float exposure = u_render_config.exposure;
	float gamma = u_render_config.gamma;
	if(exposure <= 0.)
	{
		return color;
	}
	vec3 outcol = Uncharted2Tonemap(color.rgb * exposure);
	outcol = outcol * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	return vec4(pow(outcol, vec3(1.0f / gamma)), color.a);
}
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
#define MANUAL_SRGB 1
#ifdef MANUAL_SRGB
# ifdef SRGB_FAST_APPROXIMATION
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
# else //SRGB_FAST_APPROXIMATION
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
# endif //SRGB_FAST_APPROXIMATION
	return vec4(linOut,srgbIn.w);;
#else //MANUAL_SRGB
	return srgbIn;
#endif //MANUAL_SRGB
}


#endif // USE_Render_Scene

#endif // PBR_H_