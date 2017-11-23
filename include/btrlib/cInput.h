#pragma once

#include <array>
#include <unordered_map>
#include <cstring>
#include <cctype>
#include <btrlib/Define.h>

// 0x16は使われていないのでvk_altとする
#define vk_alt 0x16u

struct cKeyboard {
	enum State {
		STATE_ON = 1 << 0,
		STATE_OFF = 1 << 1,
		STATE_HOLD = 1 << 2,
	};
	struct Param {
		uint8_t key;
		uint8_t state;
		uint8_t state_old;
		Param()
			: key(0)
			, state(0)
			, state_old(0)
		{}
	};
	std::array<Param, 256> m_data;
	bool isHold(uint8_t key)const
	{
		auto& it = m_data[key];
		return btr::isOn(it.state, STATE_HOLD);
	}
	bool isOn(uint8_t key)const
	{
		auto& it = m_data[key];
		return btr::isOn(it.state, STATE_ON);
	}
	bool isOff(uint8_t key)const
	{
		auto& it = m_data[key];
		return btr::isOn(it.state, STATE_OFF);
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

		Param()
			: x(0)
			, y(0)
			, state(0)
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
