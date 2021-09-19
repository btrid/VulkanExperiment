#version 460

#define USE_Model_Material 1

struct RenderCongig
{
	int a;
};

struct Material
{
	vec4 m_basecolor_factor;
	float m_metallic_factor;
	float m_roughness_factor;
	float _p1;
	float _p2;
	vec3  m_emissive_factor;
	float _p11;
};


layout(set=USE_Model_Material, binding=0, std140) uniform U_Material {
	Material u_material;
};
layout (set=USE_Model_Material, binding=10) uniform sampler2D t_basecolor;
layout (set=USE_Model_Material, binding=11) uniform sampler2D t_mr;
layout (set=USE_Model_Material, binding=12) uniform sampler2D t_normal;
layout (set=USE_Model_Material, binding=13) uniform sampler2D t_occlusion;
layout (set=USE_Model_Material, binding=14) uniform sampler2D t_emissive;


layout(location = 0) out vec4 FragColor;

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
	float exposure = 2.5;
	float gamma = 2.2;
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

layout(location=1) in In
{
	vec3 Texcoord_0;
}fs_in;

void main()
{
	vec4 basecolor = SRGBtoLINEAR(texture(t_basecolor, fs_in.Texcoord_0.xy)) * u_material.m_basecolor_factor;
	vec3 emissive = SRGBtoLINEAR(texture(t_emissive, fs_in.Texcoord_0.xy)).xyz * u_material.m_emissive_factor;
//	FragColor = SRGBtoLINEAR(basecolor) + SRGBtoLINEAR(vec4(emissive, 0.));
	FragColor = basecolor + vec4(emissive, 0.);
//	FragColor.xyz = emissive.xyz;
//	FragColor.xyz = basecolor.xyz;
	FragColor.w = 1.;
//	FragColor = SRGBtoLINEAR(FragColor);
}