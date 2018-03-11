#version 450

#ifdef VULKAN
#define DEPTH_ZERO_TO_ONE
#endif

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>

#define USE_OIT 0
#include <OIT.glsl>

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;


layout(location=0) in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location=1) out Transform
{
	vec3 Vertex;
} transform;


vec4 toQuat(vec3 eular)
{
	vec3 c = cos(eular * 0.5);
	vec3 s = sin(eular * 0.5);

	vec4 q;
	q.w = c.x * c.y * c.z + s.x * s.y * s.z;
	q.x = s.x * c.y * c.z - c.x * s.y * s.z;
	q.y = c.x * s.y * c.z + s.x * c.y * s.z;
	q.z = c.x * c.y * s.z - s.x * s.y * c.z;		
	return q;
}

vec3 rotate(vec4 q, vec3 v)
{
	vec3 QuatVector = q.xyz;
	vec3 uv = cross(QuatVector, v);
	vec3 uuv = cross(QuatVector, uv);
	uv *= (2. * q.w);
	uuv *= 2.;

	return v + uv + uuv;
}

mat4 orthoRH(float l, float r, float b, float t, float n, float f)
{
	mat4 o = mat4(1.);
	o[0][0] = 2. / (r - l);
	o[1][1] = 2. / (t - b);
	o[2][2] = -2. / (f - n);
	o[3][0] = - (r + l) / (r - l);
	o[3][1] = - (t + b) / (t - b);
	o[3][2] = - (f + n) / (f - n);
	return o;
}
mat4 orthoVecRH(in vec3 min, in vec3 max)
{
	return orthoRH(min.x, max.x, min.y, max.y, min.z, max.z);
}

mat4 perspectiveFovRH(in float fov, in float width, in float height, in float zNear, in float zFar)
{
	float rad = fov;
	float h = cos(0.5 * rad) / sin(0.5 * rad);
	float w = h * height / width; ///todo max(width , Height) / min(width , Height)?

	mat4  Result = mat4(0);
	Result[0][0] = w;
	Result[1][1] = h;
	Result[2][2] = - (zFar + zNear) / (zFar - zNear);
	Result[2][3] = - 1.;
	Result[3][2] = - (2. * zFar * zNear) / (zFar - zNear);
	return Result;
}
mat4 lookatRH(vec3 eye, vec3 target, vec3 up)
{
	vec3 f = normalize(target - eye);
	vec3 s = normalize(cross(f, up)); 
	vec3 u = normalize(cross(s, f));

	mat4 view = mat4(1.);
	view[0][0] = s.x;
	view[1][0] = s.y;
	view[2][0] = s.z;
	view[0][1] = u.x;
	view[1][1] = u.y;
	view[2][1] = u.z;
	view[0][2] =-f.x;
	view[1][2] =-f.y;
	view[2][2] =-f.z;
	view[3][0] =-dot(s, eye);
	view[3][1] =-dot(u, eye);
	view[3][2] = dot(f, eye);

	return view;
}

mat4 orthoLH(float l, float r, float b, float t, float n, float f)
{
	mat4 o = mat4(1.);
	o[0][0] = 2. / (r - l);
	o[1][1] = 2. / (t - b);
	o[3][0] = - (r + l) / (r - l);
	o[3][1] = - (t + b) / (t - b);

//#ifdef DEPTH_ZERO_TO_ONE
	o[2][2] = 1. / (f - n);
	o[3][2] = - n / (f - n);
//#else 
//	o[2][2] = 2. / (f - n);
//	o[3][2] = - (f + n) / (f - n);
//#endif
	return o;
}
mat4 orthoVecLH(in vec3 min, in vec3 max)
{
	return orthoLH(min.x, max.x, min.y, max.y, min.z, max.z);
}
mat4 lookatLH(vec3 eye, vec3 target, vec3 up)
{
	vec3 f = normalize(target - eye);
	vec3 s = normalize(cross(up, f)); 
	vec3 u = normalize(cross(f, s));

	mat4 view = mat4(1.);
	view[0][0] = s.x;
	view[1][0] = s.y;
	view[2][0] = s.z;
	view[0][1] = u.x;
	view[1][1] = u.y;
	view[2][1] = u.z;
	view[0][2] = f.x;
	view[1][2] = f.y;
	view[2][2] = f.z;
	view[3][0] =-dot(s, eye);
	view[3][1] =-dot(u, eye);
	view[3][2] =-dot(f, eye);
	return view;

}


void ToCellAABB(in vec3 a, in vec3 b, in vec3 c, in vec3 cellSize, out vec3 _min, out vec3 _max)
{
	_min = vec3(10e8);
	_max = vec3(-10e8);

	_max = max(_max, a);
	_max = max(_max, b);
	_max = max(_max, c);
	_min = min(_min, a);
	_min = min(_min, b);
	_min = min(_min, c);

	_max += cellSize - mod(_max, cellSize);
	_min -= mod(_min, cellSize);

}
#define PI 3.141592
#define HALF_PI 3.141592 / 2. 

void main() 
{
	
	vec3 n = normalize(cross(In[1].Position-In[0].Position, In[2].Position-In[0].Position));
	float x = dot(n, vec3(1., 0., 0.));
	float y = dot(n, vec3(0., 1., 0.));
	float z = dot(n, vec3(0., 0., 1.));

	vec3 areaMin = uParam.m_area_min.xyz;
	vec3 areaMax = uParam.m_area_max.xyz;
	vec3 size = (areaMax - areaMin);
	vec3 center = size/2. + areaMin;
	vec3 ortho_min = size/2.;
	vec3 ortho_max = -size/2.;
	vec3 eye = center;
	vec3 up = vec3(0., 1., 0.);
	mat4 projection;


	vec3 out_min, out_max;
	ToCellAABB(In[0].Position, In[1].Position, In[2].Position, size/uParam.m_cell_size.xyz, out_min, out_max);
	vec4 tMin = vec4(out_min, 0.);
	vec4 tMax = vec4(out_max, 0.);
	vec3 p[4];

	if(abs(x) > abs(y) && abs(x) > abs(z)){
		eye.x = areaMin.x;
		projection = orthoLH(ortho_min.z, ortho_max.z, ortho_min.y, ortho_max.y, 0.01, size.x);

		p[0] = tMin.xyz + tMax.www;
		p[1] = tMin.xyw + tMax.wwz;
		p[2] = tMin.wwz + tMax.xyw;
		p[3] = tMin.www + tMax.xyz;
		gl_ViewportIndex = 0;
	}
	else if(abs(y) > abs(z))
	{
		eye.y = areaMax.y;
		up = normalize(vec3(0., 0., 1.));
		projection = orthoLH(ortho_min.x, ortho_max.x, ortho_min.z, ortho_max.z, 0.01, size.y);

		p[0] = tMin.xyz + tMax.www;
		p[1] = tMin.xyw + tMax.wwz;
		p[2] = tMin.wwz + tMax.xyw;
		p[3] = tMin.www + tMax.xyz;
		gl_ViewportIndex = 1;
	}
	else
	{
		eye.z = areaMin.z;
		projection = orthoLH(ortho_min.x, ortho_max.x, ortho_min.y, ortho_max.y, 0.01, size.z);

		p[0] = tMin.xww + tMax.wyz;
		p[1] = tMin.xyz + tMax.www;
		p[2] = tMin.www + tMax.xyz;
		p[3] = tMin.wyz + tMax.xww;
		gl_ViewportIndex = 2;
	}
	mat4 view = lookatLH(eye, center, up);

	gl_Position = projection * view * vec4(p[0], 1.);
	transform.Position = p[0];
	EmitVertex();
	gl_Position = projection * view * vec4(p[1], 1.);
	transform.Position = p[1];
	EmitVertex();
	gl_Position = projection * view * vec4(p[2], 1.);
	transform.Position = p[2];
	EmitVertex();
	gl_Position = projection * view * vec4(p[3], 1.);
	transform.Position = p[3];
	EmitVertex();
	
	EndPrimitive();
}
