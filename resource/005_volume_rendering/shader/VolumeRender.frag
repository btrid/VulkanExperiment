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
//	gl_FragDepth = 1.;
	Ray fromEye = MakeRayFromScreen(u_eye.xyz, u_target.xyz, In.texcoord, u_fov_y, u_aspect);

	{
		Hit hit = marchToAABB(fromEye, u_scene.u_volume_min.xyz, u_scene.u_volume_max.xyz);
		if(hit.IsHit == 0)
		{
			// フォグエリアに当たらなかった
			FragColor = vec4(0.);
//			return;
			discard;

		}
		else
		{
			fromEye.p = hit.HitPoint + fromEye.d * 0.01;
		}
	}

	vec3 volume_albedo = vec3(1.);
	vec3 color = vec3(0.0, 0.0, 0.0);
	float alpha = 0.;
	while(isInAABB(fromEye.p, u_scene.u_volume_min.xyz, u_scene.u_volume_max.xyz) && alpha < 1.)
	{
		//Volumeの取得
		vec3 p = fromEye.p - u_scene.u_volume_min.xyz;
		float density = texture(u_volume, p / volumeSize).r;
		{
			// レイマーチ
			alpha = alpha + (1.-alpha)*density;
			vec3 march = fromEye.d * marchDist;
			fromEye.p += march;
		}

		// ライトからの照度を計算
		Ray toLight;
		toLight.p = fromEye.p;
		toLight.d = normalize(u_scene.u_light_pos.xyz - toLight.p);
		float translucent = 0.; // 光がどの程度届いたか
		while(isInAABB(toLight.p, u_scene.u_volume_min.xyz, u_scene.u_volume_max.xyz) && translucent<1.)
		{
			vec3 lp = toLight.p - u_scene.u_volume_min.xyz;
			float ldensity = texture(u_volume, lp / volumeSize).r;
			translucent = translucent + (1.-translucent)*ldensity;
			vec3 march = toLight.d * marchDist;
			toLight.p += march;
		}

		vec3 diffuse = volume_albedo * u_scene.u_light_color.xyz*(1.-translucent);
		color += diffuse * density;

	}
	vec3 ambient= vec3(0.2);
	FragColor = vec4(mix(vec3(0., 0., 0.), color, alpha) + ambient, alpha);
//	vec4 p = uProjection * uView * vec4(pos, 1.);
//	p /= p.w;
//	gl_FragDepth = p.z;
//	FragColor = vec4(1.);
}

