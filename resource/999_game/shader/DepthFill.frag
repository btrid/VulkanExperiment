#version 450

out float gl_FragDepth;
in vec4 gl_FragCoord;

void main()
{
	// depth 書くだけ
	// glslc バグってるのか、明示的に書かないとFragmentShaderが起動しない？ while(true);も無視される
	//	while(true);
	gl_FragDepth = gl_FragCoord.z;

}