#version 450
#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require

#include </convertDimension.glsl>
#include </Shape.glsl>
#include </PMDefine.glsl>
#include </Brick.glsl>
#include </PM.glsl>


layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;


in gl_PerVertex
{
	vec4 gl_Position;
//	float gl_PointSize;
//	float gl_ClipDistance[];
} gl_in[];

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

in InVertex{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID;
}In[];

out Transform{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

layout(std140, binding = 0) uniform BrickUniform
{
	BrickParam uParam;
};

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

mat4 ortho(float l, float r, float b, float t, float n, float f)
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
mat4 orthoVec(in vec3 min, in vec3 max)
{
	return ortho(min.x, max.x, min.y, max.y, min.z, max.z);
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

mat4 lookat(vec3 eye, vec3 target, vec3 up)
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

	vec3 areaMin = uParam.areaMin;
	vec3 areaMax = uParam.areaMax;
	vec3 size = (areaMax - areaMin);
	vec3 center = size/2. + areaMin;
	vec3 eye = center;
	vec3 up = vec3(0., 1., 0.);
	mat4 projection;


	vec3 _min, _max;
	ToCellAABB(In[0].Position, In[1].Position, In[2].Position, size/vec3(BRICK_SUB_SIZE), _min, _max);
	vec4 tMin = vec4(_min, 0.);
	vec4 tMax = vec4(_max, 0.);
	vec3 p[4];

	if(abs(x) > abs(y) && abs(x) > abs(z)){
//		eye.x = x > 0 ? areaMax.x : areaMin.x;
		eye.x = areaMin.x;
		projection = ortho(areaMin.z, areaMax.z, areaMin.y, areaMax.y, 0, size.x);

		p[0] = tMin.xyz + tMax.www;
		p[1] = tMin.xyw + tMax.wwz;
		p[2] = tMin.wwz + tMax.xyw;
		p[3] = tMin.www + tMax.xyz;
	}
	else if(abs(y) > abs(z))
	{
//		eye.y = y > 0 ? areaMax.y : areaMin.y;
		eye.y = areaMax.y;
		up = normalize(vec3(0., 0., 1.));
		projection = ortho(areaMin.x, areaMax.x, areaMin.z, areaMax.z, 0, size.y);

		p[0] = tMin.xyz + tMax.www;
		p[1] = tMin.xyw + tMax.wwz;
		p[2] = tMin.wwz + tMax.xyw;
		p[3] = tMin.www + tMax.xyz;
	}
	else
	{
		eye.z = areaMin.z;
		projection = ortho(areaMin.x, areaMax.x, areaMin.y, areaMax.y, 0, size.z);

		p[0] = tMin.xww + tMax.wyz;
		p[1] = tMin.xyz + tMax.www;
		p[2] = tMin.www + tMax.xyz;
		p[3] = tMin.wyz + tMax.xww;
	}
	mat4 view = lookat(eye, center, up);

	transform.VertexID[0] = In[0].VertexID;
	transform.VertexID[1] = In[1].VertexID;
	transform.VertexID[2] = In[2].VertexID;
	transform.DrawID = In[0].DrawID;
	transform.InstanceID = In[0].InstanceID;

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
