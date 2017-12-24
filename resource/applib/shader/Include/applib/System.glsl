
#ifndef BTRLIB_SYSTEM_GLSL
#define BTRLIB_SYSTEM_GLSL

#define isOn(_a, _b) (((_a)&(_b))==(_b))
//#define isAll(_a, _b) ((_a&_b)==_b)
#define isAny(_a, _b) (((_a)&(_b))!=0)
#define isOff(_a, _b) (((_a)&(_b))==0)
#define setOn(_a, _b) (_a|=_b)
#define setOff(_a, _b) (_a&=~_b)

#define setSwap(_a, _b) \
if(isOn((_a),(_b))) setOff((_a),(_b)); \
else setOn((_a),(_b))

#define setBit(_a, _b, _c) \
if(_c) setOn((_a), (_b)); \
else setOff((_a), (_b))

#define getBit(_bit, _bitoffset, _bitrange) \
(((((1 << (_bitrange)) - 1) << (_bitoffset)) & (_bit)) >> (_bitoffset))

ivec2 i32Toi16(in int i32)
{
	int hi = getBit(i32, 16, 15);
	hi = (isOn(i32, 1<<31)? -(0x8000-hi) : hi);
	int lo = getBit(i32, 0, 15);
	lo = (isOn(i32, 1<<15) ? -(0x8000 - lo) : lo);
	return ivec2(lo, hi);
}
ivec4 i32Toi8(in int i32)
{
	int v0 = getBit(i32, 0, 7);
	v0 = (isOn(i32, 1<<7)? -(0x80-v0) : v0);
	int v1 = getBit(i32, 8, 7);
	v1 = (isOn(i32, 1<<15)? -(0x80-v1) : v1);
	int v2 = getBit(i32, 16, 7);
	v2 = (isOn(i32, 1<<23)? -(0x80-v2) : v2);
	int v3 = getBit(i32, 24, 7);
	v3 = (isOn(i32, 1<<31)? -(0x80-v3) : v3);
	return ivec4(v0, v1, v2, v3);
}


#define KEY_BIT_UP (1<<0)
#define KEY_BIT_DOWN (1<<1)
#define KEY_BIT_RIGHT (1<<2)
#define KEY_BIT_LEFT (1<<3)
#define KEY_BIT_L (1<<4)
#define KEY_BIT_K (1<<5)

#define MOUSE_LEFT_ON (1<<0)
#define MOUSE_LEFT_OFF (1<<1)
#define MOUSE_LEFT_HOLD (1<<2)
struct SystemData
{
	uint m_gpu_index;
	uint m_gpu_frame;
	float m_deltatime;
	uint _p13;

	uvec2 m_resolution;
	ivec2 m_mouse_position;

	ivec2 m_mouse_position_old;
	uint m_is_mouse_on;
	uint m_is_mouse_off;

	uint m_is_mouse_hold;
	float m_touch_time;
	uint _p22;
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