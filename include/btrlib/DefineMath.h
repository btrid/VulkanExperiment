#pragma once

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

using u64 = std::uint64_t;
using s64 = std::int64_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

using f16x2 = uint16_t;
using s8x4 = glm::i8vec4;
using u8x4 = glm::u8vec4;
namespace btr
{
template<typename A, typename B>
bool isOn(A bit, B test) {
	return (bit & test) == test;
}
template<typename A, typename B>
bool isOff(A bit, B test) {
	return (bit & test) == 0;
}

template<typename A, typename B>
void setOn(A& bit, B test) {
	bit |= test;
}

template<typename A, typename B>
void setOff(A& bit, B test) {
	bit &= ~test;
}

}