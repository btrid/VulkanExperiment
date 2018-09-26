#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

float rate[] = {1., 1.};

layout(location = 0) out vec4 FragColor;
void main()
{
	ivec2 reso = u_gi2d_info.m_resolution;
	int hierarchy = u_gi2d_scene.m_hierarchy;
	uint radiance_size = (reso.x*reso.y)>>(hierarchy*2);
	vec3 radiance = vec3(0.);

//	for(int i = 0; i<2; i++) 
	{
		uvec4 coord_x = (ivec4(gl_FragCoord.x)>>hierarchy) + ivec4(0,1,0,1);
		uvec4 coord_y = (ivec4(gl_FragCoord.y)>>hierarchy) + ivec4(0,0,1,1);
		uvec4 coord = getMemoryOrder4(coord_x, coord_y);
		ivec3 subcoord_i = ivec3(gl_FragCoord.xy, 0)%(1<<hierarchy);
		vec3 subcoord =  subcoord_i / float(1<<hierarchy);
//		ivec2 framecoord = ivec2(u_gi2d_scene.m_frame%2, u_gi2d_scene.m_frame/2) - ivec2(1);
		vec3 radiance_ = vec3(0.);
		int count = 0;
//		for(int y = 0; y < Block_Size; y++){
//		for(int x = 0; x < Block_Size; x++){
//			coord += + ivec2(x-(Block_Size>>1), y-(Block_Size>>1));
//			coord = clamp(coord, ivec2(0), reso>>hierarchy);
			for(int i = 0; i < 4; i++)
			{
				vec3 rad[4];
				rad[0] = unpackEmissive(b_radiance[coord.x]);
				rad[1] = unpackEmissive(b_radiance[coord.y]);
				rad[2] = unpackEmissive(b_radiance[coord.z]);
				rad[3] = unpackEmissive(b_radiance[coord.w]);
				radiance_ += mix(mix(rad[0], rad[1], subcoord.x), mix(rad[2], rad[3], subcoord.x), subcoord.y);
				coord += radiance_size;
			}
			count += 8;
//		}}
		radiance += count==0 ? radiance_ : (radiance_ / count)*4;
	}

	ivec2 map_index = ivec2(gl_FragCoord.xy);
	ivec2 shift = map_index%8;
	ivec2 _fi = map_index/8;
	int findex = _fi.x + _fi.y*(u_gi2d_info.m_resolution.x/8);
	uint64_t fragment_map = b_diffuse_map[findex.x];
	bool is_fragment = (fragment_map & 1ul<<(shift.x+shift.y*8)) != 0;
	int fragment_index = map_index.x + map_index.y * u_gi2d_info.m_resolution.x;

	radiance *= b_fragment[fragment_index].albedo.xyz;

	FragColor = vec4(radiance, 1.);

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

