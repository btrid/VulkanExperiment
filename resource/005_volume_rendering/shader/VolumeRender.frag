#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
//#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <convertDimension.glsl>
#include <Shape.glsl>
#include <Marching.glsl>

struct VolumeScene
{
	vec4 u_volume_min;
	vec4 u_volume_max;

	vec4 u_light_pos;
	vec4 u_light_color;
};

#define SETPOINT_VOLUME 0
layout(set=SETPOINT_VOLUME, binding = 0)uniform sampler3D u_volume;
layout(set=SETPOINT_VOLUME, binding = 1, std140) uniform VolumeUniform{
	VolumeScene u_scene;
};

#define SETPOINT_CAMERA 1
#include <Camera.glsl>

layout(location=1) in FSIn{
	vec2 texcoord;
}In;
layout(location = 0) out vec4 FragColor;




void main() 
{
	vec3 volumeSize = (u_scene.u_volume_max.xyz - u_scene.u_volume_min.xyz);
	ivec3 texSize = textureSize(u_volume, 0);
	vec3 marchDist = volumeSize / (texSize*3.);

	Ray fromEye = MakeRayFromScreen(u_eye.xyz, u_target.xyz, In.texcoord, u_fov_y, u_aspect);

	vec3 endpoint = fromEye.p;
	vec3 startpoint = (fromEye.p + fromEye.d * 9999.);
	{
		Ray toAABB;
		toAABB.p = startpoint;
		toAABB.d = -fromEye.d;
		Hit hit = marchToAABB(toAABB, u_scene.u_volume_min.xyz, u_scene.u_volume_max.xyz);
		if(hit.IsHit == 0)
		{
			// フォグエリアに当たらなかった
			FragColor = vec4(0.);
			return;
			discard;

		}
		else
		{
			startpoint = hit.HitPoint + toAABB.d * 0.01;
		}
	}
	Ray toEye;
	toEye.p = startpoint;
	toEye.d = -fromEye.d;

	vec3 volumeColor = vec3(1.);
	vec3 diffuse = vec3(0.2, 0.2, 0.9);
	float totalpass = 1.;
	while(isInAABB(toEye.p, u_scene.u_volume_min.xyz, u_scene.u_volume_max.xyz) && dot(toEye.d, endpoint-toEye.p) >= 0.)
	{
		vec3 p = toEye.p - u_scene.u_volume_min.xyz;
		float density = texture(u_volume, p / volumeSize).r;
		{
			totalpass = clamp(totalpass-density, 0., 1.);
			vec3 march = toEye.d * marchDist;
			toEye.p += march;
		}

		// ライトからの照度を計算
		Ray toLight;
		toLight.p = u_scene.u_light_pos.xyz;
		toLight.d = normalize(toEye.p - toLight.p);
		Hit hit = marchToAABB(toLight, u_scene.u_volume_min.xyz, u_scene.u_volume_max.xyz);
		toLight.p = hit.HitPoint + toLight.d * 0.01;
		float pass = 1.;
		while(isInAABB(toLight.p, u_scene.u_volume_min.xyz, u_scene.u_volume_max.xyz) && dot(toLight.d, toEye.p-toLight.p) >= 0.)
		{
			vec3 lp = toLight.p - u_scene.u_volume_min.xyz;
			float ldensity = texture(u_volume, lp / volumeSize).r;
			pass = clamp(pass - ldensity, 0., 1.);
			vec3 march = toLight.d * marchDist;
			toLight.p += march;
		}

		vec3 color = mix(volumeColor, u_scene.u_light_color.xyz, pass);
		diffuse = mix(diffuse, color, density);

	}
	vec3 color = diffuse;
	FragColor = vec4(color, 1.);
//	FragColor = vec4(1.);
}

