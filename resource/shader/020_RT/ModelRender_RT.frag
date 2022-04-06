#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : require

#define M_PI 3.141592

#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

#define USE_Render_Scene 1
#include "pbr.glsl"

#define USE_Model_Resource 2
#define USE_Model_Render 3
#include "Model.glsl"


layout(location=1) in PerVertex
{
	vec3 WorldPos;
	float _d1;
	vec3 Normal;
	float _d2;
	vec2 Texcoord_0;
	flat uint64_t MaterialAddress;
	flat uint meshletID;
}In;
layout(location = 0) out vec4 FragColor;
 // Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo
{
	float NdotL;                  // cos angle between normal and light direction
	float NdotV;                  // cos angle between normal and view direction
	float NdotH;                  // cos angle between normal and half vector
	float LdotH;                  // cos angle between light direction and half vector

	float VdotH;                  // cos angle between view direction and half vector
	float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
	float metalness;              // metallic value at the surface

	vec3 reflectance0;            // full reflectance color (normal incidence angle)

	vec3 reflectance90;           // reflectance color at grazing angle
	float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])

	vec3 diffuseColor;            // color contribution from diffuse lighting

	vec3 specularColor;           // color contribution from specular lighting

	vec3 N;
	vec3 V;
};

Material u_material;
vec3 u_light_dir;

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
	return In.Normal;
	// Perturb normal, see http://www.thetenthplanet.de/archives/1180
	if(u_material.TexID_Normal>-1)
	{

	}

	vec3 tangentNormal = texture(t_ModelTexture[u_material.TexID_Normal], In.Texcoord_0).xyz * 2.0 - 1.0;

	vec3 q1 = dFdx(In.WorldPos.xyz);
	vec3 q2 = dFdy(In.WorldPos.xyz);
	vec2 st1 = dFdx(In.Texcoord_0);
	vec2 st2 = dFdy(In.Texcoord_0);

	vec3 N = normalize(In.Normal.xyz);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

// Calculation of the lighting contribution from an optional Image Based Light source.
// Precomputed Environment Maps are required uniform inputs and are computed as outlined in [1].
// See our README.md on Environment Maps [3] for additional discussion.
vec3 getIBLContribution(PBRInfo pbrInputs, vec3 n, vec3 reflection)
{
	float lod = pbrInputs.perceptualRoughness * float(textureQueryLevels(t_environment_prefiltered));
//	float lod = 0.;
	// retrieve a scale and bias to F0. See [1], Figure 3
	vec3 brdf = texture(t_brdf_lut, vec2(pbrInputs.NdotV, 1.0 - pbrInputs.perceptualRoughness)).rgb;
	vec3 diffuseLight = SRGBtoLINEAR(tonemap(texture(t_environment_irradiance, n))).rgb;
	vec3 specularLight = SRGBtoLINEAR(tonemap(textureLod(t_environment_prefiltered, reflection, lod))).rgb;
	vec3 diffuse = diffuseLight * pbrInputs.diffuseColor;
	vec3 specular = specularLight * (pbrInputs.specularColor * brdf.x + brdf.y);

	// For presentation, this allows us to disable IBL terms
	diffuse *= u_render_config.ambient_power;
	specular *= u_render_config.ambient_power;

	return diffuse + specular;
}


// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(PBRInfo pbrInputs)
{
	return pbrInputs.diffuseColor / M_PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
	return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
// https://qiita.com/emadurandal/items/76348ad118c36317ec5c
float geometricOcclusion(PBRInfo pbrInputs)
{
	float NdotL = pbrInputs.NdotL;
	float NdotV = pbrInputs.NdotV;
	float r = pbrInputs.alphaRoughness;

	float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
	float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
	return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
	float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
	float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
	return roughnessSq / (M_PI * f * f);
}


vec3 DirectLight()
{
	float t = 0.0001;
	float tmax = 99999.;
	rayQueryEXT rq;
	rayQueryInitializeEXT(rq, u_TLAS_Scene, gl_RayFlagsOpaqueEXT, 0xFF, In.WorldPos.xyz, t, u_light_dir, tmax);
	while(rayQueryProceedEXT(rq)) {}

	t = rayQueryGetIntersectionTEXT(rq, true);
	float LightPower = t>=tmax ? 1. : 0.;

#define LightColor vec3(10.)
	return LightPower * LightColor;
};

struct QueryMaterial
{

};
QueryMaterial Sample(vec3 ray)
{
	QueryMaterial material;
	float t = 0.0001;
	float tmax = 999.;
	rayQueryEXT rq;
	rayQueryInitializeEXT(rq, u_TLAS_Scene, gl_RayFlagsOpaqueEXT, 0xFF, In.WorldPos.xyz, t, ray, tmax);
	while(rayQueryProceedEXT(rq)) {}

	t = rayQueryGetIntersectionTEXT(rq, true);
	if(t>=tmax){ return material;} // hitしない

	rayQueryGetIntersectionInstanceCustomIndexEXT(rq, true);
};

// Based omn http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random(vec2 co)
{
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dt= dot(co.xy ,vec2(a,b));
	float sn= mod(dt,3.14);
	return fract(sin(sn) * c);
}

vec2 hammersley2d(uint i, uint N) 
{
	// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	uint bits = (i << 16u) | (i >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return vec2(float(i) /float(N), rdi);
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
vec3 importanceSample_GGX(vec2 Xi, float roughness, vec3 normal) 
{
	// Maps a 2D point to a hemisphere with spread based on roughness
	float alpha = roughness * roughness;
	float phi = 2.0 * M_PI * Xi.x /*+ random(normal.xz) * 0.1*/;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

	// Tangent space
	vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangentX = normalize(cross(up, normal));
	vec3 tangentY = normalize(cross(normal, tangentX));

	// Convert to world Space
	return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

// Geometric Shadowing function
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}


vec2 BRDF(PBRInfo pbrInputs)
{
	vec2 LUT = vec2(0.0);
#define NUM_SAMPLES 4
	for(uint i = 0u; i < NUM_SAMPLES; i++) {
		vec2 Xi = hammersley2d(i, NUM_SAMPLES);
		vec3 H = importanceSample_GGX(Xi, pbrInputs.alphaRoughness, pbrInputs.N);
		vec3 L = 2.0 * dot(pbrInputs.V, H) * H - pbrInputs.V;

		float dotNL = max(dot(pbrInputs.N, L), 0.0);
		float dotNV = max(dot(pbrInputs.N, pbrInputs.V), 0.0);
		float dotVH = max(dot(pbrInputs.V, H), 0.0); 
		float dotNH = max(dot(H, pbrInputs.N), 0.0);

		if (dotNL > 0.0) {
			float G = G_SchlicksmithGGX(dotNL, dotNV, pbrInputs.alphaRoughness);
			float G_Vis = (G * dotVH) / (dotNH * dotNV);
			float Fc = pow(1.0 - dotVH, 5.0);
			LUT += vec2((1.0 - Fc) * G_Vis, Fc * G_Vis);
		}
	}
	return LUT / float(NUM_SAMPLES);
}

vec3 LightSampling(PBRInfo pbrInputs)
{
	float t = 0.0001;
	float tmax = 99999.;
	rayQueryEXT rq;
	rayQueryInitializeEXT(rq, u_TLAS_Scene, gl_RayFlagsOpaqueEXT, 0xFF, In.WorldPos.xyz, t, u_light_dir, tmax);
	while(rayQueryProceedEXT(rq)) {}

	t = rayQueryGetIntersectionTEXT(rq, true);
	float LightPower = t>=tmax ? 1. : 0.;

#define LightColor vec3(10.)
	return LightPower * LightColor;
};



// 0度でのフレネル反射率
const vec3 f0 = vec3(0.04);



void main()
{
	u_light_dir = normalize(vec3(0.2, 1.5, 0.2));
	MaterialBuffer mat  = MaterialBuffer(In.MaterialAddress);
	u_material = mat.m[0];
	vec4 basecolor = u_material.m_basecolor_factor;
	if(u_material.TexID_Base>-1)
	{
		basecolor *= SRGBtoLINEAR(texture(t_ModelTexture[u_material.TexID_Base], In.Texcoord_0.xy));
	}

	float perceptualRoughness = u_material.m_roughness_factor;
	float metallic = u_material.m_metallic_factor;
	if(u_material.TexID_MR > -1)
	{
		vec4 mrSample = texture(t_ModelTexture[u_material.TexID_MR], In.Texcoord_0.xy);
		perceptualRoughness *= mrSample.g;
		metallic *= mrSample.b;
	}

	vec3 diffuseColor = basecolor.rgb * (vec3(1.0) - f0);
	diffuseColor *= 1.0 - metallic;

	vec3 specularColor = mix(f0, basecolor.xyz, metallic);
	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

	float alphaRoughness = perceptualRoughness * perceptualRoughness;
	
	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;


	vec3 n = getNormal();
	vec3 v = normalize(u_camera[0].u_eye.xyz - In.WorldPos.xyz);
	vec3 l = u_light_dir;
	vec3 h = normalize(l+v);
	vec3 reflection = -normalize(reflect(v, n));
	reflection.y *= -1.0f;

	PBRInfo pbrInputs;

	pbrInputs.NdotL = clamp(dot(n, l), 0.001, 1.0);
	pbrInputs.NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
	pbrInputs.NdotH = clamp(dot(n, h), 0.0, 1.0);
	pbrInputs.LdotH = clamp(dot(l, h), 0.0, 1.0);
	pbrInputs.VdotH = clamp(dot(v, h), 0.0, 1.0);
	pbrInputs.perceptualRoughness = perceptualRoughness;
	pbrInputs.metalness = metallic;
	pbrInputs.reflectance0 = specularEnvironmentR0;
	pbrInputs.reflectance90 = specularEnvironmentR90;
	pbrInputs.alphaRoughness = alphaRoughness;
	pbrInputs.diffuseColor = diffuseColor;
	pbrInputs.specularColor = specularColor;
	pbrInputs.N = n;
	pbrInputs.V = v;

	vec3 F = specularReflection(pbrInputs);
	float G = geometricOcclusion(pbrInputs);
	float D = microfacetDistribution(pbrInputs);

	// Calculation of analytical lighting contribution
	vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
	vec3 specContrib = F * G * D / (4.0 * pbrInputs.NdotL * pbrInputs.NdotV);
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
	vec3 color = /*pbrInputs.NdotL * DirectLight() * */ (diffuseContrib + specContrib);
//	color += getIBLContribution(pbrInputs, n, reflection);

	const float u_OcclusionStrength = 1.0f;
	// Apply optional PBR terms for additional (optional) shading
	if (u_material.TexID_AO > -1) 
	{
		float ao = texture(t_ModelTexture[nonuniformEXT(u_material.TexID_AO)], In.Texcoord_0).r;
		color = mix(color, color * ao, u_OcclusionStrength);
	}

	if (u_material.TexID_Emissive > -1) 
	{
		vec3 emissive = SRGBtoLINEAR(texture(t_ModelTexture[nonuniformEXT(u_material.TexID_Emissive)], In.Texcoord_0)).rgb * u_material.m_emissive_factor;
		color += emissive;
	}
	
	FragColor = vec4(color, 1.);
}