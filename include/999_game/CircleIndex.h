#pragma once

template<typename T, size_t Size>
struct CircleIndex
{
	T m_circle_index;
	CircleIndex() :m_circle_index() {}

	void increment() { m_circle_index = (m_circle_index + 1) % Size; }
	void decrement() { m_circle_index = ((m_circle_index == 0) ? Size - 1 : m_circle_index - 1); }

	T get()const { return m_circle_index; }
	T getPrev()const { return (m_circle_index == 0) ? Size - 1 : m_circle_index - 1; }


	T operator++(int) { T i = m_circle_index; increment(); return i; }
	T operator++() { increment(); return m_circle_index; }
	T operator--(int) { T i = m_circle_index; increment(); return i; }
	T operator--() { decrement(); return m_circle_index; }
	T operator()()const { return get(); }

};
