#version 450
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require


layout(location=0) out vec4 FragColor;

void main()
{
	FragColor.rgb = vec3(1.);
	FragColor.a = 1.;
}