#pragma once

#include <array>
#include <unordered_map>
#include <btrlib/Define.h>
struct cKeyboard {
	enum State {
		STATE_ON = 1 << 0,
		STATE_OFF = 1 << 1,
		STATE_HOLD = 1 << 2,
	};
	struct Param {
		char key;
		int state;

		Param()
			: key(0)
			, state(0)
		{}
	};
	std::unordered_map<WPARAM, Param> mData;
	bool isHold(char key)const 
	{ 
		auto it = mData.find(key);
		if (it == mData.end()) {
			return false;
		}
		return btr::isOn(it->second.state, STATE_HOLD);
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
		STATE_OFF = 1 << 1,		//!< 押している間は立つ
		STATE_HOLD = 1 << 2,	//!< 離したフレームのみ立つ
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
	bool isHold(Button button)const { return btr::isOn(m_param[button].state, STATE_HOLD); }
};

struct cInput
{
	cKeyboard m_keyboard;
	cMouse m_mouse;
};
