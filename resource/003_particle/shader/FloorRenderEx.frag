#version 450
#pragma optionNV (unroll none)
#pragma optionNV (inline all)

//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#include </Shape.glsl>
//#include </Marching.glsl>

#define SETPOINT_CAMERA 0
#include <Common.glsl>

#define SETPOINT_MAP 1
#include </Map.glsl>


layout(location=1) in FSIn{
	vec2 texcoord;
}In;
layout(location = 0) out vec4 FragColor;


vec3 getCellSize3D()
{
	return vec3(u_map_info.cell_size.x, WALL_HEIGHT, u_map_info.cell_size.y);
}
vec3 clampByMapIndex(in vec3 pos, in ivec3 map_index)
{
	vec3 cell_size = getCellSize3D();
	float particle_size = 0.;
	return clamp(pos, (vec3(map_index) * cell_size)+particle_size+FLT_EPSIRON, (map_index+ivec3(1)) * cell_size-particle_size-FLT_EPSIRON);
}
ivec3 calcMapIndex(in vec3 p)
{
	vec3 cell_size = vec3(u_map_info.cell_size.x, WALL_HEIGHT, u_map_info.cell_size.y);
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




void main() 
{
	vec3 foward = normalize(u_target - u_eye).xyz;
	vec3 side = cross(vec3(0., 1., 0.), foward);
	side = dot(side, side) <= 0.1 ? vec3(1., 0., 0.) : normalize(side);
	vec3 up = normalize(cross(side, foward));

	// イメージセンサー上の位置
	float tan_fov_y = tan(u_fov_y / 2.);

	vec3 position_on_sensor =
		(u_eye.xyz + foward)
		+ side * -In.texcoord.x * tan_fov_y * u_aspect
		+ up * In.texcoord.y * tan_fov_y;
	// レイを飛ばす方向。
	vec3 dir = normalize(position_on_sensor - u_eye.xyz);
	vec3 pos = u_eye.xyz;

	Ray ray = MakeRay(pos, dir);
	vec2 area = u_map_info.m_cell_num * u_map_info.cell_size;
	vec3 bmin = vec3(0) - 0.1;
	vec3 bmax = vec3(area.x, WALL_HEIGHT, area.y) + 0.1;
	Hit hit = marchToAABB(ray, bmin, bmax);

	gl_FragDepth = 1.;
	if(hit.IsHit == 0)
	{
		FragColor = vec4(0., 0., 0., 1.);
		return;
	}
	pos = hit.HitPoint;
	ivec3 map_index = calcMapIndex(pos);
	for(;;)
	{
		ivec3 next = marchEx3D(pos, map_index, dir);
		if(any(lessThan(map_index.xz, ivec2(0)))
		|| any(greaterThanEqual(map_index.xz, ivec2(u_map_info.m_cell_num))))
		{
			FragColor = vec4(0., 0., 0., 1.);
			return;
		}
		uint map = imageLoad(t_map, map_index.xz).x;
		vec3 p = pos + next *1.;
		if(next.y != 0)
		{
			// 地面と当たった場合
			if(p.y <= map*WALL_HEIGHT)
			{
				FragColor = vec4(map*1., 0., 1., 1.);
				break;
			}
		}
		else
		{
			if(p.y <= map*WALL_HEIGHT)
			{
				// 壁と当たった場合
				FragColor = vec4(1., 0., 0., 1.);
				break;
			}
		}
	}
	vec4 p = uProjection * uView * vec4(pos, 1.);
	p /= p.w;
	gl_FragDepth = p.z;

}
