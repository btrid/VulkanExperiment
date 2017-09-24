#version 450
#pragma optionNV (unroll none)
#pragma optionNV (inline all)

//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#include </Math.glsl>
#include </Shape.glsl>
//#include </Marching.glsl>

#define SETPOINT_CAMERA 0
#include <Camera.glsl>
#include <Common.glsl>

#define SETPOINT_MAP 1
#define SETPOINT_SCENE 2
#include </Map.glsl>

layout(location=1) in FSIn{
	vec2 texcoord;
}In;
layout(location = 0) out vec4 FragColor;

ivec3 calcMapIndex(in MapDescriptor desc, in vec3 p)
{
	vec3 cell_size = vec3(desc.m_cell_size.x, WALL_HEIGHT, desc.m_cell_size.y);
	ivec3 map_index = ivec3(p.xyz / cell_size);
	return map_index;
}

float calcDepthStandard(in float v, in float n, in float f)
{
	return n * f / (v * (f - n) - f);
}
float calcReverseDepth(in float v, in float n, in float f)
{
	return -calcDepthStandard(v, f, n);
}
float calcDepth(in float v, in float n, in float f)
{
	return 1.-calcDepthStandard(v, n, f);
}

vec3 proj(in vec3 x, in vec3 normal)
{
	return dot(x, normal) / dot(normal, normal) * normal;
}


Hit marchToAABB(in Ray ray, in vec3 bmin, in vec3 bmax)
{
	Hit ret = MakeHit();

	if(all(lessThan(ray.p, bmax)) 
	&& all(greaterThan(ray.p, bmin)))
	{
		// AABBの中にいる
		ret.IsHit = 1;
		ret.HitPoint = ray.p;
		ret.Distance = 0;
		return ret;
	}

	float tmin = 0.;
	float tmax = 10e6;
	for (int i = 0; i < 3; i++)
	{
		if (abs(ray.d[i]) < 10e-6)
		{
			// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
			if (ray.p[i] < bmin[i] || ray.p[i] > bmax[i])
			{
				return ret;
			}
		}
		else
		{
			float ood = 1. / ray.d[i];
			float t1 = (bmin[i] - ray.p[i]) * ood;
			float t2 = (bmax[i] - ray.p[i]) * ood;

			// t1が近い平面との交差、t2が遠い平面との交差になる
			float near = min(t1, t2);
			float far = max(t1, t2);

			// スラブの交差している感覚との交差を計算
			tmin = max(near, tmin);
			tmax = max(far, tmax);

			if (tmin > tmax) {
				return ret;
			}
		}
	}
	float dist = tmin;
	ret.IsHit = 1;
	ret.HitPoint = ray.p + ray.d*dist;
	ret.Distance = dist;
	return ret;

}

float calcEmission(in vec3 pos)
{
	float power = 0.2;
	float start_time_offset = dot(pos, pos) / length(pos);
//	start_time_offset += rand3(pos)*20;
	float time = u_scene_data.m_totaltime*50. - start_time_offset;
	if(time <= 0.){ return power;}
	time = mod(time/10, 20.);
	time = min(time, 1.);
	power += 1-pow(time, 0.8);
	return min(power, 1.);
	
}

void main() 
{
	MapDescriptor desc = u_map_info.m_descriptor[0];

	vec3 foward = normalize(u_camera[0].u_target - u_camera[0].u_eye).xyz;
	vec3 side = cross(vec3(0., 1., 0.), foward);
	side = dot(side, side) <= 0.1 ? vec3(1., 0., 0.) : normalize(side);
	vec3 up = normalize(cross(side, foward));

	// イメージセンサー上の位置
	float tan_fov_y = tan(u_camera[0].u_fov_y / 2.);

	vec3 position_on_sensor =
		(u_camera[0].u_eye.xyz + foward)
		+ side * -In.texcoord.x * tan_fov_y * u_camera[0].u_aspect
		+ up * In.texcoord.y * tan_fov_y;
	// レイを飛ばす方向。
	vec3 dir = normalize(position_on_sensor - u_camera[0].u_eye.xyz);
	vec3 pos = u_camera[0].u_eye.xyz;

	Ray ray = MakeRay(pos, dir);
	vec2 area = desc.m_cell_num * desc.m_cell_size;
	vec3 bmin = vec3(0) - 0.1;
	vec3 bmax = vec3(area.x, WALL_HEIGHT*5, area.y) + 0.1;
	Hit hit = marchToAABB(ray, bmin, bmax);

	if(hit.IsHit == 0)
	{
		discard;
		FragColor = vec4(0., 0., 0., 1.);
		return;
	}

	pos = hit.HitPoint;
	ivec3 map_index = calcMapIndex(desc, pos);
	for(;;)
	{
		ivec3 next = marchEx3D(pos, map_index, dir);
		if(any(lessThan(map_index.xz, ivec2(0)))
		|| any(greaterThanEqual(map_index.xz, ivec2(desc.m_cell_num))))
		{
			discard;
		}
		uint map = imageLoad(t_map, map_index.xz).x;
		vec3 p = pos + next *1.;
		if(next.y != 0)
		{
			// 地面と当たった場合
			if(p.y <= map*WALL_HEIGHT)
			{
				FragColor = vec4(vec3(map*1., 0., 1.) * calcEmission(pos), 1.);
				break;
			}
		}
		else
		{
			if(p.y <= map*WALL_HEIGHT)
			{
				// 壁と当たった場合
//				FragColor = vec4(1., 0., 0., 1.);
				FragColor = vec4(vec3(1., 0., 0.) * calcEmission(pos), 1.);
				break;
			}
		}
	}
	vec4 p = u_camera[0].u_projection * u_camera[0].u_view * vec4(pos, 1.);
	p /= p.w;
	gl_FragDepth = p.z;

}
