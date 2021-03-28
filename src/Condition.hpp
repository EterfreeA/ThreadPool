/*
* �ļ����ƣ�Condition.hpp
* ժҪ��
* 1.����C++17��׼������������std::condition_variable��װ������ģ��Condition��
* 2.����ٽ������������������Ҳ���ν����Ϊ��������֮����wait�Ĳ�����ȷ�������Ѿ��������̣߳������߳��ڵȴ�֮ǰ��ν��Ϊ�������������
*	��������֮����notify�������ӳ�֪ͨ���ر��ھ�ȷ�����¼�֮ʱ���޷�ȷ�������ڵ���notify֮��ͨ������wait���������̡߳�
* 
* �汾��v1.0
* ���ߣ����
* ���䣺2592419242@qq.com
* �������ڣ�2021��03��13��
* �������ڣ�2021��03��21��
*/

#pragma once

#include <chrono>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "core.h"

ETERFREE_BEGIN

template <typename _Size = std::size_t>
class Condition
{
	std::mutex _mutex;
	std::atomic_bool _validity;
	std::condition_variable _condition;

public:
	Condition() : _validity(true) {}

	Condition(const Condition&) = delete;

	~Condition() { exit(); }

	Condition& operator=(const Condition&) = delete;

	bool valid() const noexcept { return _validity.load(std::memory_order::memory_order_relaxed); }

	void exit();

	void notify_one();

	void notify_all();

	void notify(_Size _size);

	template <typename _Predicate>
	void notify_one(_Predicate _predicate);

	template <typename _Predicate>
	void notify_all(_Predicate _predicate);

	template <typename _Predicate>
	void notify(_Size _size, _Predicate _predicate);

	void wait();

	template <typename _Predicate>
	void wait(_Predicate _predicate);

	template <typename _Rep, typename _Period>
	bool wait_for(std::chrono::duration<_Rep, _Period>& _relative);

	template <typename _Rep, typename _Period, typename _Predicate>
	bool wait_for(std::chrono::duration<_Rep, _Period>& _relative, _Predicate _predicate);

	template <typename _Clock, typename _Duration>
	bool wait_until(const std::chrono::time_point<_Clock, _Duration>& _absolute);

	template <typename _Clock, typename _Duration, typename _Predicate>
	bool wait_until(const std::chrono::time_point<_Clock, _Duration>& _absolute, _Predicate _predicate);

	bool wait_until(const xtime* const _absolute);

	template <typename _Predicate>
	bool wait_until(const xtime* const _absolute, _Predicate _predicate);
};

template <typename _Size>
void Condition<_Size>::exit()
{
	std::unique_lock lock(_mutex);
	if (!valid())
		return;

	_validity.store(false, std::memory_order::memory_order_relaxed);
	lock.unlock();
	_condition.notify_all();
}

template <typename _Size>
void Condition<_Size>::notify_one()
{
	std::lock_guard lock(_mutex);
	_condition.notify_one();
}

template <typename _Size>
void Condition<_Size>::notify_all()
{
	std::lock_guard lock(_mutex);
	_condition.notify_all();
}

template <typename _Size>
void Condition<_Size>::notify(_Size _size)
{
	std::lock_guard lock(_mutex);
	for (decltype(_size) index = 0; index < _size; ++index)
		_condition.notify_one();
}

template <typename _Size>
template <typename _Predicate>
void Condition<_Size>::notify_one(_Predicate _predicate)
{
	std::unique_lock lock(_mutex);
	if (_predicate())
	{
		lock.unlock();
		_condition.notify_one();
	}
}

template <typename _Size>
template <typename _Predicate>
void Condition<_Size>::notify_all(_Predicate _predicate)
{
	std::unique_lock lock(_mutex);
	if (_predicate())
	{
		lock.unlock();
		_condition.notify_all();
	}
}

template <typename _Size>
template <typename _Predicate>
void Condition<_Size>::notify(_Size _size, _Predicate _predicate)
{
	std::unique_lock lock(_mutex);
	if (_predicate)
	{
		lock.unlock();
		for (decltype(_size) index = 0; index < _size; ++index)
			_condition.notify_one();
	}
}

template <typename _Size>
void Condition<_Size>::wait()
{
	std::unique_lock lock(_mutex);
	if (valid())
		_condition.wait(lock);
}

template <typename _Size>
template <typename _Predicate>
void Condition<_Size>::wait(_Predicate _predicate)
{
	std::unique_lock lock(_mutex);
	_condition.wait(lock, [this, &_predicate] { return !valid() || _predicate(); });
}

template <typename _Size>
template <typename _Rep, typename _Period>
bool Condition<_Size>::wait_for(std::chrono::duration<_Rep, _Period>& _relative)
{
	std::unique_lock lock(_mutex);
	return !valid() || _condition.wait_for(lock, _relative) == std::cv_status::no_timeout;
}

template <typename _Size>
template <typename _Rep, typename _Period, typename _Predicate>
bool Condition<_Size>::wait_for(std::chrono::duration<_Rep, _Period>& _relative, _Predicate _predicate)
{
	std::unique_lock lock(_mutex);
	return _condition.wait_for(lock, _relative, [this, &_predicate] { return !valid() || _predicate(); });
}

template <typename _Size>
template <typename _Clock, typename _Duration>
bool Condition<_Size>::wait_until(const std::chrono::time_point<_Clock, _Duration>& _absolute)
{
	std::unique_lock lock(_mutex);
	return !valid() || _condition.wait_until(lock, _absolute) == std::cv_status::no_timeout;
}

template <typename _Size>
template <typename _Clock, typename _Duration, typename _Predicate>
bool Condition<_Size>::wait_until(const std::chrono::time_point<_Clock, _Duration>& _absolute, _Predicate _predicate)
{
	std::unique_lock lock(_mutex);
	return _condition.wait_until(lock, _absolute, [this, &_predicate] { return !valid() || _predicate(); });
}

template <typename _Size>
bool Condition<_Size>::wait_until(const xtime* const _absolute)
{
	std::unique_lock lock(_mutex);
	return !valid() || _condition.wait_until(lock, _absolute) == std::cv_status::no_timeout;
}

template <typename _Size>
template <typename _Predicate>
bool Condition<_Size>::wait_until(const xtime* const _absolute, _Predicate _predicate)
{
	std::unique_lock lock(_mutex);
	return _condition.wait_until(lock, _absolute, [this, &_predicate] { return !valid() || _predicate(); });
}

ETERFREE_END
