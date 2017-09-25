#include <btrlib/Math.glsl>

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

struct DrawIndirectCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};
