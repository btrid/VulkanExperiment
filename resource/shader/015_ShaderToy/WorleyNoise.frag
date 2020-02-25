#version 460
#extension GL_GOOGLE_include_directive : require

layout(push_constant) uniform Point
{
	vec2 reso;
	vec2 center;
} point;

layout(location=0) out vec4 FragColor;

void main()
{
	vec2 uv = gl_PointCoord.xy * 2. - 1.;
	float dist = dot(uv, vec2(1.));
	FragColor = vec4(dist.xxx, 1.);
//	FragColor = vec4(0., 0., 1., 1.);
}