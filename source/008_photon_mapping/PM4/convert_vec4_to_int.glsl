
vec4 convert_uint_to_vec4(in uint value)
{
	return vec4(float((value & 0x000000FF)), 
	float((value & 0x0000FF00) >> 8U), 
	float((value & 0x00FF0000) >> 16U), 
	float((value & 0xFF000000) >> 24U));
}

uint convert_vec4_to_uint(in vec4 value)
{
	return
	((uint(value.w) & 0x000000FF) << 24U)
	| ((uint(value.z) & 0x000000FF) << 16U)
	| ((uint(value.y) & 0x000000FF) << 8U)
	| ((uint(value.x) & 0x000000FF));
}
