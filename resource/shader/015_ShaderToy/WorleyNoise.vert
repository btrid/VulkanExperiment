#version 460
#extension GL_GOOGLE_include_directive : require

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
//	float gl_PointSize;
};

layout(push_constant) uniform Point
{
	vec2 reso;
	vec2 center;
} point;

void main()
{
//	point.center = rand(float(gl_VertexIndex));
//	point.size = 5.;
	gl_Position = vec4(point.center, 0, 1);
//	gl_PointSize = 10.;
}
