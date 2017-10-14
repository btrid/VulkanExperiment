#ifndef BTRLIB_COMMON_GLSL
#define BTRLIB_COMMON_GLSL

#define quat vec4
quat angleAxis(in float angle, in vec3 v)
{
	quat Result;

	float s = sin(angle * 0.5);
	Result.w = cos(angle * 0.5);
	Result.x = v.x * s;
	Result.y = v.y * s;
	Result.z = v.z * s;
	return Result;
}

float calcAngle(in vec3 x, in vec3 y)
{
	return acos(clamp(dot(x, y), -1., 1));
}

vec3 rotateQuatVec3(in quat q,	in vec3 v)
{

	vec3 quat_vector = q.xyz;
	vec3 uv = cross(quat_vector, v);
	vec3 uuv = cross(quat_vector, uv);

	return v + ((uv * q.w) + uuv) * 2.;
}

#define FLT_EPSIRON (0.0001)
struct DrawIndirectCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

struct DrawElementIndirectCommand{
	uint  count;
	uint  instanceCount;
	uint  firstIndex;
	uint  baseVertex;
	uint  baseInstance;
};


float rand(in vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
vec4 rand4(in vec2 co){
	return vec4(rand(co*sin(co.x*co.y)), rand(co.yx), rand(co*sin(co.y)), rand(co*sin(co.x)));
}

#endif