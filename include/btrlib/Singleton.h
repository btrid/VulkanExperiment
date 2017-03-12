#pragma once

struct SingletonStorageStatic
{
	template<typename T>
	static T& Get()
	{
		static T instance;
		return instance;
	}
};

struct SingletonStorageThreadLocal
{
	template<typename T>
	static T& Get()
	{
		thread_local T instance;
		return instance;
	}
};
template <
	typename T,
	typename Storage
>
class Singleton_t
{
public:
	struct U : public T {};
public:
	static T& Order() {
// 		static T instance;
//		return instance;
		return Storage::Get<U>();
	}

protected:
	Singleton_t() = default;
	~Singleton_t() = default;
private:
	Singleton_t(const Singleton_t& rhv) = delete;
	Singleton_t(Singleton_t &&) = delete;
	const Singleton_t& operator = (const Singleton_t& single) = delete;
	Singleton_t& operator=(Singleton_t &&) = delete;
public:
};

template<typename T> using Singleton = Singleton_t<T, SingletonStorageStatic>;
template<typename T> using SingletonTLS = Singleton_t<T, SingletonStorageThreadLocal>;
//#define SingletonTLS Singleton

