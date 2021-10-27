#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_nonuniform_qualifier : require


layout(location=1) in Vertex
{
	vec3 WorldPos;
	vec3 Normal;
	vec2 Texcoord_0;
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

	uint TexID_Base;
	uint TexID_MR;
	uint TexID_AO;
	uint TexID_Normal;

	uint TexID_Emissive;
	uint Tex_Base_;
	uint Tex_Bas_e;
	uint Tex_Ba_se;

};
Material u_material;

struct Entity
{
	uint64_t Vertex;
	uint64_t Normal;

	uint64_t Texcoord;
	uint64_t Index;

	uint64_t Material;
	uint64_t Material_Index;

};


#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

#define USE_Render_Scene 1
#include "pbr.glsl"

#define USE_Model_Resource 2
layout(set=USE_Model_Resource, binding=0, scalar) buffer EntityBuffer { Entity b_entity[]; };
layout(set=USE_Model_Resource, binding=1, buffer_reference, scalar) buffer V {vec3 b_v[]; };
layout(set=USE_Model_Resource, binding=2, buffer_reference, scalar) buffer I {vec3 b_i[]; };
layout(set=USE_Model_Resource, binding=3, buffer_reference, scalar) buffer MaterialBuffer {Material m[]; };

#define USE_Model_Texture 3
layout (set=USE_Model_Texture, binding=0) uniform sampler2D t_ModelTexture[];




#define USE_Model_Entity 4
layout(set=USE_Model_Entity, binding=0, scalar) buffer ModelEntityBuffer { Entity b_model_entity[]; };


// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
	// Perturb normal, see http://www.thetenthplanet.de/archives/1180
	vec3 tangentNormal = texture(t_ModelTexture[nonuniformEXT(u_material.TexID_Normal)], In.Texcoord_0).xyz * 2.0 - 1.0;

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

// Gets metallic factor from specular glossiness workflow inputs 
/*
float convertMetallic(vec3 diffuse, vec3 specular, float maxSpecular) 
{
	float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
	float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);
	if (perceivedSpecular < c_MinRoughness) 
	{
		return 0.0;
	}
	float a = c_MinRoughness;
	float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - c_MinRoughness) + perceivedSpecular - 2.0 * c_MinRoughness;
	float c = c_MinRoughness - perceivedSpecular;
	float D = max(b * b - 4.0 * a * c, 0.0);
	return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}
*/


// 0度でのフレネル反射率
const vec3 f0 = vec3(0.04);



void main()
{
	Entity entity = b_model_entity[0];
	MaterialBuffer mat  = MaterialBuffer(entity.Material);
	u_material = mat.m[0];
	vec4 basecolor = SRGBtoLINEAR(texture(t_ModelTexture[nonuniformEXT(u_material.TexID_Base)], In.Texcoord_0.xy)) * u_material.m_basecolor_factor;

	// Metallic and Roughness material properties are packed together
	// In glTF, these factors can be specified by fixed scalar values
	// or from a metallic-roughness map

	// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
	// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
	vec4 mrSample = texture(t_ModelTexture[nonuniformEXT(u_material.TexID_MR)], In.Texcoord_0.xy);
	float perceptualRoughness = mrSample.g * u_material.m_roughness_factor;
	float metallic = mrSample.b * u_material.m_metallic_factor;

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
	vec3 l = normalize(vec3(0.6, -1.5, 0.5));
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

	vec3 F = specularReflection(pbrInputs);
	float G = geometricOcclusion(pbrInputs);
	float D = microfacetDistribution(pbrInputs);

	// Calculation of analytical lighting contribution
	vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
	vec3 specContrib = F * G * D / (4.0 * pbrInputs.NdotL * pbrInputs.NdotV);
	// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
#define LightColor vec3(1.)
	vec3 color = pbrInputs.NdotL * LightColor * (diffuseContrib + specContrib);

	// Calculate lighting contribution from image based lighting source (IBL)
	color += getIBLContribution(pbrInputs, n, reflection);

	const float u_OcclusionStrength = 1.0f;
	// Apply optional PBR terms for additional (optional) shading
//	if (material.occlusionTextureSet > -1) 
	{
		float ao = texture(t_ModelTexture[nonuniformEXT(u_material.TexID_AO)], In.Texcoord_0).r;
		color = mix(color, color * ao, u_OcclusionStrength);
	}

//	if (material.emissiveTextureSet > -1) 
	{
		vec3 emissive = SRGBtoLINEAR(texture(t_ModelTexture[nonuniformEXT(u_material.TexID_Emissive)], In.Texcoord_0)).rgb * u_material.m_emissive_factor;
		color += emissive;
	}
	
	FragColor = vec4(color, basecolor.a);
}