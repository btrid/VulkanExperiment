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
#include </Voxel.glsl>
#include </PM.glsl>


layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

in GSIN
{
	vec3 Position;
	vec3 Normal;
	vec4 Texcoord; //[u,v][u,v]
	flat int MaterialIndex;
}In[];

out GSOUT
{
	vec3 Position;
	vec3 Normal;
	vec4 Texcoord; //[u,v][u,v]
	flat int MaterialIndex;
}GSOut;

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
#define PI 3.141592
#define HALF_PI 3.141592 / 2. 

void main() 
{
	vec3 n = normalize(cross(In[1].Position-In[0].Position, In[2].Position-In[0].Position));
	float x = dot(n, vec3(1., 0., 0.));
	float y = dot(n, vec3(0., 1., 0.));
	float z = dot(n, vec3(0., 0., 1.));

	vec3 areaMin = BRICK_AREA_MIN;
	vec3 areaMax = BRICK_AREA_MAX;
	vec3 size = (areaMax - areaMin);
	vec3 center = size/2. + areaMin;
	vec3 eye = center;
	vec3 up = vec3(0., 1., 0.);
	mat4 projection;

	if(abs(x) > abs(y) && abs(x) > abs(z)){
		eye.x = x > 0 ? areaMax.x : areaMin.x;
		projection = ortho(areaMin.z, areaMax.z, areaMin.y, areaMax.y, 0, size.x);
	}
	else if(abs(y) > abs(z))
	{
		eye.y = y > 0 ? areaMax.y : areaMin.y;
		up = normalize(vec3(0., 0., 1.));
		projection = ortho(areaMin.x, areaMax.x, areaMin.z, areaMax.z, 0, size.y);
	}
	else
	{
		eye.z = z > 0 ? areaMax.z : areaMin.z;
		projection = ortho(areaMin.x, areaMax.x, areaMin.y, areaMax.y, 0, size.z);
	}
	mat4 view = lookat(eye, center, up);

	gl_Position = projection * view * vec4(In[0].Position, 1.);
	GSOut.Position = In[0].Position;
	GSOut.Normal = In[0].Normal;
	GSOut.Texcoord = In[0].Texcoord;
	GSOut.MaterialIndex = In[0].MaterialIndex;
	EmitVertex();

	gl_Position = projection * view * vec4(In[1].Position, 1.);
	GSOut.Position = In[1].Position;
	GSOut.Normal = In[1].Normal;
	GSOut.Texcoord = In[1].Texcoord;
	EmitVertex();

	gl_Position = projection * view * vec4(In[2].Position, 1.);
	GSOut.Position = In[2].Position;
	GSOut.Normal = In[2].Normal;
	GSOut.Texcoord = In[2].Texcoord;
	EmitVertex();
	
	EndPrimitive();
}
