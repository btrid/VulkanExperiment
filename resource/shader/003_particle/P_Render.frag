#version 460
#extension GL_GOOGLE_include_directive : require

layout(location=1) in PerVertex
{
	vec3 WorldPos;
	flat int Type;
}In;

layout(location = 0) out vec4 FragColor;

uint xor(uint y) 
{
    y = y ^ (y << 13); y = y ^ (y >> 17);
    return y = y ^ (y << 5);
}

vec3 colortable(uint id)
{
	vec3 colors[] = {
		vec3(0., 0., 0.),
		vec3(1., 1., 1.),
		vec3(1., 0., 0.),
		vec3(0.5, 0.5, 0.),
	};

	return colors[id];
}

void main()
{
	FragColor = vec4(colortable(In.Type), 1.);
}