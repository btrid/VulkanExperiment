#pragma once

#include <array>
#include <unordered_map>
#include <cstring>
#include <cctype>
#include <btrlib/Define.h>

#include <bitset>
struct cKeyboard
{
	std::bitset<256> m_state;
	std::bitset<256> m_state_old;
	std::bitset<256> m_is_on;
	std::bitset<256> m_is_off;
	std::array<wchar_t, 16> m_char;
	uint32_t m_char_count = 0;
	bool isHold(uint8_t key)const
	{
		return m_state[key];
	}
	bool isOn(uint8_t key)const
	{
		return m_is_on[key];
	}
	bool isOff(uint8_t key)const
	{
		return m_is_off[key];
	}
};

struct cMouse
{
	enum Button{
		BUTTON_LEFT,
		BUTTON_RIGHT,
		BUTTON_MIDDLE,
		BUTTON_NUM,
	};
	enum State{
		STATE_ON = 1 << 0,		//!< 押したフレームのみ立つ
		STATE_OFF = 1 << 1,		//!< 離したフレームのみ立つ
		STATE_HOLD = 1 << 2,	//!< 押している間は立つ
	};
	struct Param {
		int x, y;
		int state;
		float time;
		Param()
			: x(0)
			, y(0)
			, state(0)
			, time(0.f)
		{}
	};
	std::array<Param, BUTTON_NUM> m_param;
	glm::ivec2 xy;
	glm::ivec2 xy_old;
	int32_t wheel;

	cMouse() 
		: wheel(0)
	{}
	int32_t getWheel()const { return wheel; }
	glm::ivec2 getMove()const { return xy_old - xy; }
	bool isOn(Button button)const { return btr::isOn(m_param[button].state, STATE_ON); }
	bool isOff(Button button)const { return btr::isOn(m_param[button].state, STATE_OFF); }
	bool isHold(Button button)const { return btr::isOn(m_param[button].state, STATE_HOLD); }
};

struct cInput
{
	cKeyboard m_keyboard;
	cMouse m_mouse;
};
