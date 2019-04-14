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
using uint = std::uint32_t;

using vec4 = glm::vec4;
using vec3 = glm::vec3;
using vec2 = glm::vec2;
using vec1 = glm::vec1;
using ivec4 = glm::ivec4;
using ivec3 = glm::ivec3;
using ivec2 = glm::ivec2;
using ivec1 = glm::ivec1;
using uvec4 = glm::uvec4;
using uvec3 = glm::uvec3;
using uvec2 = glm::uvec2;
using uvec1 = glm::uvec1;
using mat4 = glm::mat4;
using mat3 = glm::mat3;
using mat2 = glm::mat2;
using i64vec4 = glm::i64vec4;
using u64vec4 = glm::u64vec4;
using i64vec2 = glm::i64vec2;
using u64vec2 = glm::u64vec2;
using i16vec2 = glm::i16vec2;
using u16vec2 = glm::u16vec2;
using i8vec4 = glm::i8vec4;
using u8vec4 = glm::u8vec4;

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
	return (bit & test) != test;
}

template<typename A, typename B>
void setOn(A& bit, B test) {
	bit |= test;
}

template<typename A, typename B>
void setOff(A& bit, B test) {
	bit &= ~test;
}
template<typename A, typename B>
void setSwap(A& bit, B test) {
	if (isOn(bit, test)) {
		setOff(bit, test);
	}
	else {
		setOn(bit, test);
	}
}
template<typename A, typename B>
void setBit(A& bit, B value, uint32_t bitoffset, uint32_t bitrange) {
	uint32_t mask = (1 << bitrange) - 1;
	assert(value < mask); // bitrange‚ÉŽû‚Ü‚Á‚Ä‚È‚¢‚Ì‚ÍƒoƒO‚Ì‰Â”\«‚ª‚‚¢‚Ì‚ÅŽ~‚ß‚é
	mask <<= bitoffset;
	bit = ((~mask & bit) + value) << bitoffset;
}
template<typename A>
A getBit(A& bit, uint32_t bitoffset, uint32_t bitrange) {
	uint32_t mask = ((1 << bitrange) - 1) << bitoffset;
	return (mask & bit) >> bitoffset;
}

//!	align(4, 32) -> 32
//!	align(33, 32) -> 64
template<typename T>
T align(T value, T align_num)
{
	return (value + (align_num-1)) & ~(align_num -1);
}

}