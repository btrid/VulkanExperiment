
#ifndef BTRLIB_SYSTEM_GLSL
#define BTRLIB_SYSTEM_GLSL

#define KEY_BIT_UP (1<<0)
#define KEY_BIT_DOWN (1<<1)
#define KEY_BIT_RIGHT (1<<2)
#define KEY_BIT_LEFT (1<<3)
#define KEY_BIT_L (1<<4)
#define KEY_BIT_K (1<<5)

struct SystemData
{
	uint m_gpu_index;
	uint m_gpu_frame;
	float m_deltatime;
	uint _p13;
	ivec2 m_mouse_position;
	ivec2 m_mouse_position_old;
	uint m_is_mouse_on;
	uint m_is_mouse_off;
	uint m_is_mouse_hold;
	uint _p23;

	uint m_is_key_on;
	uint m_is_key_off;
	uint m_is_key_hold;
	uint _p33;

};

#ifdef USE_SYSTEM
layout(set=USE_SYSTEM, binding=0, std140) uniform SystemUniform {
	SystemData u_system_data;
};
#endif

#endif