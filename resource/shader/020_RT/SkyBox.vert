#version 460
#extension GL_GOOGLE_include_directive : require


layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};


void main()
{
	gl_Position = vec4(1.);
}
