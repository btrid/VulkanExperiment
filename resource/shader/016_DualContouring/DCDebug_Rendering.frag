#version 460
#extension GL_GOOGLE_include_directive : require



layout(location=1) in Transform{
	flat uvec3 CellID;
}transform;


layout(location = 0) out vec4 FragColor;
void main()
{
	FragColor = vec4(1.);

}