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
	const Singleton_t& operator = (const Singleton_t&) = delete;
	Singleton_t& operator=(Singleton_t &&) = delete;
public:
};

// template<typename T> using Singleton = Singleton_t<T, SingletonStorageStatic>;
 template<typename T> using SingletonTLS = Singleton_t<T, SingletonStorageThreadLocal>;

#include <mutex>
template <
	typename T
>
class Singleton
{
public:
	struct U : public T {};
	static U* s_instance;
	static std::mutex s_m;
public:
	static T& Order() 
	{
		if (!s_instance)
		{
			std::lock_guard<std::mutex> lg(s_m);
			if (!s_instance)
			{
				s_instance = new U;
			}
		}
		return *s_instance;
	}

protected:
	Singleton() = default;
	~Singleton() = default;
private:
	Singleton(const Singleton& rhv) = delete;
	Singleton(Singleton &&) = delete;
	const Singleton& operator = (const Singleton&) = delete;
	Singleton& operator=(Singleton &&) = delete;
public:
};
template<typename T>
typename Singleton<T>::U* Singleton<T>::s_instance;
template<typename T>
std::mutex Singleton<T>::s_m;

template <
	typename T
>
class SingletonEx
{
public:
	static T* s_instance;
	static std::mutex s_m;
public:
	template<typename... Args>
	static void Create(Args... args)
	{
		if (!s_instance)
		{
			std::lock_guard<std::mutex> lg(s_m);
			if (!s_instance)
			{
				s_instance = new T(args...);
			}
		}

	}
	static void Release()
	{
		std::lock_guard<std::mutex> lg(s_m);
		delete s_instance;
		s_instance = nullptr;
	}

	static T& Order()
	{
		return *s_instance;
	}

protected:
	SingletonEx() = default;
	~SingletonEx() = default;
private:
	SingletonEx(const SingletonEx& rhv) = delete;
	SingletonEx(SingletonEx &&) = delete;
	const SingletonEx& operator = (const SingletonEx&) = delete;
	SingletonEx& operator=(SingletonEx &&) = delete;
public:
};
template<typename T>
typename T* SingletonEx<T>::s_instance;
template<typename T>
std::mutex SingletonEx<T>::s_m;


// thread_local U* s_instance;
// thread_local std::mutex s_m;
// template <
// 	typename T
// >
// class SingletonTLS
// {
// public:
// 	struct U : public T {};
// public:
// 	static T& Order() {
// 		if (!s_instance)
// 		{
// 			std::lock_guard<std::mutex> lg(mm);
// 			if (!s_instance)
// 			{
// 				s_instance = new U;
// 			}
// 		}
// 		return *s_instance;
// 	}
// 
// protected:
// 	SingletonTLS() = default;
// 	~SingletonTLS() = default;
// private:
// 	SingletonTLS(const SingletonTLS& rhv) = delete;
// 	SingletonTLS(SingletonTLS &&) = delete;
// 	const SingletonTLS& operator = (const SingletonTLS&) = delete;
// 	SingletonTLS& operator=(SingletonTLS &&) = delete;
// public:
// };
// template<typename T>
// SingletonTLS<T>::U* SingletonTLS<T>::s_instance;
// template<typename T>
// std::mutex SingletonTLS<T>::s_m;

