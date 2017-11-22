#pragma once

#include <array>
#include <unordered_map>
#include <cstring>
#include <cctype>
#include <btrlib/Define.h>
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
	std::array<Param, 255> m_data;
	bool isHold(char key)const
	{
		auto& it = m_data[std::toupper(key)];
		return btr::isOn(it.state, STATE_HOLD);
	}
	bool isOn(char key)const
	{
		auto& it = m_data[std::toupper(key)];
		return btr::isOn(it.state, STATE_ON);
	}
	bool isOff(char key)const
	{
		auto& it = m_data[std::toupper(key)];
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
		STATE_ON = 1 << 0,		//!< �������t���[���̂ݗ���
		STATE_OFF = 1 << 1,		//!< �������t���[���̂ݗ���
		STATE_HOLD = 1 << 2,	//!< �����Ă���Ԃ͗���
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
