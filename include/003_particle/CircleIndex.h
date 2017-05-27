#pragma once

template<typename T, size_t Size>
struct CircleIndex
{
	T m_circle_index;
	CircleIndex() :m_circle_index() {}

	T operator++(int) { T i = m_circle_index; this->operator++(); return i; }
	T operator++() { m_circle_index = (m_circle_index + 1) % Size; return m_circle_index; }
	T operator--(int) { T i = m_circle_index; this->operator--(); return i; }
	T operator--() { m_circle_index = (m_circle_index == 0) ? Size - 1 : m_circle_index - 1; return m_circle_index; }
	T operator()()const { return m_circle_index; }

	T get()const { return m_circle_index; }
};
