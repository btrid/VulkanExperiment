#pragma once

#include <array>
#include <vector>
#include <atomic>
#include <memory>

// マルチスレッドからpush, reserveを呼べる固定長バッファ
// get / push reserve 非スレッドセーフ
template<typename T, uint32_t N>
struct AppendBuffer
{
	AppendBuffer() : m_offset(0) {}

	void push(const T* data, uint32_t num)
	{
		auto* buf = reserve(num);
		memcpy_s(buf, num * sizeof(T), data, num * sizeof(T));
	}
	T* reserve(uint32_t num)
	{
		auto index = m_offset.fetch_add(num);
		assert(index + num < N); // 取りすぎ

		return m_buffer.data() + index;
	}

	bool empty()const { return m_offset == 0; }
	std::vector<T> get()
	{
		// thread_safeではない
		auto num = m_offset.exchange(0);
		std::vector<T> ret(num);
		memcpy_s(ret.data(), num * sizeof(T), m_buffer.data(), num * sizeof(T));
		return ret;
	}

private:
	std::array<T, N> m_buffer;
	std::atomic_uint32_t m_offset;

};
