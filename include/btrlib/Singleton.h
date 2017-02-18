#pragma once
/*
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
*/
template <
	typename T
>
class Singleton
{
public:
	static T& Order() {
		static T instance;
		return instance;
	}

protected:
	Singleton() = default;
	~Singleton() = default;
private:
	Singleton(const Singleton& rhv) = delete;
	Singleton(Singleton &&) = delete;
	const Singleton& operator = (const Singleton& single) = delete;
	Singleton& operator=(Singleton &&) = delete;
public:
};

//template<typename T> using Singleton = Singleton_t<T/*, SingletonStorageStatic*/>;
//template<typename T> using SingletonTLS = Singleton_t<T/*, SingletonStorageThreadLocal*/>;
//#define SingletonTLS Singleton

