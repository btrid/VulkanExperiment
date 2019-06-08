#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

layout(location=1)in InData
{
	vec4 color;
};

layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 radiance = vec3(1.);
	FragColor = vec4(radiance, 1.);
	return;

	// tonemapテスト
	{
//#define max_luminamce (0.009)
//#define avg_luminamce (0.000004)
#define max_luminamce (50.)
#define avg_luminamce (0.000004)

		const vec3 RGB2Y   = vec3( +0.29900, +0.58700, +0.11400);
		const vec3 RGB2Cb  = vec3( -0.16874, -0.33126, +0.50000);
		const vec3 RGB2Cr  = vec3( +0.50000, -0.41869, -0.08131);
		const vec3 YCbCr2R = vec3( +1.00000, +0.00000, +1.40200);
		const vec3 YCbCr2G = vec3( +1.00000, -0.34414, -0.71414);
		const vec3 YCbCr2B = vec3( +1.00000, +1.77200, +0.00000);

		// YCbCr系に変換
		vec3 YCbCr;
		YCbCr.y = dot(RGB2Cb, radiance);
		YCbCr.z = dot(RGB2Cr, radiance);

		// 色の強さは補正
		float coeff = 0.18 * exp( -avg_luminamce ) * max_luminamce;
		float lum = coeff * dot(RGB2Y, radiance);
		YCbCr.x = lum * (1.+lum/(max_luminamce*max_luminamce)) / (1.+lum);

		// RGB系にして出力
		vec4 color;
		color.r = dot( YCbCr2R,  YCbCr );
		color.g = dot( YCbCr2G,  YCbCr );
		color.b = dot( YCbCr2B,  YCbCr );
		color.a = 1.;
		FragColor = color;
 	}
}

