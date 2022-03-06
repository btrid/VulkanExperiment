#version 460
#extension GL_GOOGLE_include_directive : require

layout(location=1) in PerVertex
{
	vec3 WorldPos;
	vec3 Normal;
	flat uvec2 MaterialID;
}In;

layout(location = 0) out vec4 FragColor;

uint xor(uint y) 
{
    y = y ^ (y << 13); y = y ^ (y >> 17);
    return y = y ^ (y << 5);
}

vec3 colortable(uint id)
{
	uint r1 = xor(id+100);
	uint r2 = xor(r1);
	uint r3 = xor(r2);
	return fract(sin(vec3(r1, r2, r3)*0.063147));
}
void main()
{
	uint id = In.MaterialID[int(gl_FrontFacing)];
//	uint id = In.MaterialID.x;
	if (id == 0) discard;

	vec3 L = normalize(vec3(1.));
    float NL = dot(normalize(In.Normal), L);
	vec3 color = colortable(id);
	FragColor = vec4(clamp(color * 0.4 + color * 0.6 /** NL*/, 0.0, 1.0), 1.);
}