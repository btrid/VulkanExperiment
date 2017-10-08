#pragma once

template<typename T, uint32_t N>
struct AppendBuffer
{
	AppendBuffer() : m_offset(0) {}

	void push(const T* data, size_t num)
	{
		auto* buf = reserve(num);
		memcpy_s(buf, num * sizeof(T), data, num * sizeof(T));
	}
	T* reserve(size_t num)
	{
		auto index = m_offset.fetch_add(num);
		assert(index + num < N);

		return m_buffer.data() + index;
	}

	bool empty()const { return m_offset == 0; }
	std::vector<T> get()
	{
		// thread_safe‚Å‚Í‚È‚¢
		auto num = m_offset.exchange(0);
		std::vector<T> ret(num);
		memcpy_s(ret.data(), num * sizeof(T), m_buffer.data(), num * sizeof(T));
		return ret;
	}

private:
	std::array<T, N> m_buffer;
	std::atomic_uint32_t m_offset;

};
