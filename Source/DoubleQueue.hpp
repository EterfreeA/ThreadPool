﻿/*
* 文件名称：DoubleQueue.hpp
* 语言标准：C++17
* 
* 创建日期：2019年03月08日
* 更新日期：2023年01月07日
* 
* 摘要
* 1.定义双缓冲队列类模板DoubleQueue。
* 2.包含入口队列和出口队列。在放入元素之时，只锁定入口互斥元；在取出元素之时，先锁定出口互斥元，若出口队列为空，再锁定入口互斥元，并且交换两个队列。
*   以此降低两个队列的相互影响，从而提高出入队列的效率。
* 3.支持自定义队列容量和动态调整容量，支持批量出入队列和清空队列。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v2.0.0
* 变化
* v2.0.0
* 1.更名Queue为DoubleQueue。
* 2.定制复制语义和移动语义。
*/

#pragma once

#include <optional>
#include <utility>
#include <list>
#include <atomic>
#include <mutex>

#include "Common.hpp"

ETERFREE_SPACE_BEGIN

template <typename _Element>
class DoubleQueue final
{
public:
	using Element = _Element;
	using QueueType = std::list<Element>;
	using SizeType = typename QueueType::size_type;
	using MutexType = std::mutex;

private:
	using Atomic = std::atomic<SizeType>;

private:
	Atomic _capacity;
	Atomic _size;

	mutable MutexType _entryMutex;
	QueueType _entryQueue;

	mutable MutexType _exitMutex;
	QueueType _exitQueue;

private:
	static auto get(const Atomic& _atomic) noexcept
	{
		return _atomic.load(std::memory_order_relaxed);
	}

	static void set(Atomic& _atomic, SizeType _size) noexcept
	{
		_atomic.store(_size, std::memory_order_relaxed);
	}

	static auto exchange(Atomic& _atomic, SizeType _size) noexcept
	{
		return _atomic.exchange(_size, std::memory_order_relaxed);
	}

	static void copy(DoubleQueue& _left, const DoubleQueue& _right);

	static void move(DoubleQueue& _left, DoubleQueue&& _right) noexcept;

private:
	auto add(SizeType _size) noexcept
	{
		return this->_size.fetch_add(_size, \
			std::memory_order_relaxed);
	}

	auto subtract(SizeType _size) noexcept
	{
		return this->_size.fetch_sub(_size, \
			std::memory_order_relaxed);
	}

	bool valid(QueueType& _queue) const noexcept;

public:
	// 若_capacity小于等于零，则无限制，否则其为上限值
	DoubleQueue(SizeType _capacity = 0) : \
		_capacity(_capacity), _size(0) {}

	DoubleQueue(const DoubleQueue& _another);

	DoubleQueue(DoubleQueue&& _another);

	DoubleQueue& operator=(const DoubleQueue& _doubleQueue);

	DoubleQueue& operator=(DoubleQueue&& _doubleQueue);

	auto capacity() const noexcept
	{
		return get(_capacity);
	}

	void reserve(SizeType _capacity) noexcept
	{
		set(this->_capacity, _capacity);
	}

	auto size() const noexcept { return get(_size); }
	bool empty() const noexcept { return size() == 0; }

	DEPRECATED
	auto& mutex() noexcept { return _exitMutex; }

	std::optional<SizeType> push(const Element& _element);
	std::optional<SizeType> push(Element&& _element);

	std::optional<SizeType> push(QueueType& _queue);
	std::optional<SizeType> push(QueueType&& _queue);

	bool pop(Element& _element);
	std::optional<Element> pop();

	bool pop(QueueType& _queue);

	SizeType clear();
};

template <typename _Element>
void DoubleQueue<_Element>::copy(DoubleQueue& _left, \
	const DoubleQueue& _right)
{
	_left._exitQueue = _right._exitQueue;
	_left._entryQueue = _right._entryQueue;
	set(_left._size, get(_right._size));
	set(_left._capacity, get(_right._capacity));
}

template <typename _Element>
void DoubleQueue<_Element>::move(DoubleQueue& _left, \
	DoubleQueue&& _right) noexcept
{
	_left._exitQueue = std::move(_right._exitQueue);
	_left._entryQueue = std::move(_right._entryQueue);
	set(_left._size, exchange(_right._size, 0));
	set(_left._capacity, exchange(_right._capacity, 0));
}

template <typename _Element>
bool DoubleQueue<_Element>::valid(QueueType& _queue) const noexcept
{
	auto capacity = this->capacity();
	if (capacity <= 0) return true;

	auto size = this->size();
	return size < capacity && _queue.size() <= capacity - size;
}

template <typename _Element>
DoubleQueue<_Element>::DoubleQueue(const DoubleQueue& _another)
{
	std::scoped_lock lock(_another._exitMutex, \
		_another._entryMutex);
	copy(*this, _another);
}

template <typename _Element>
DoubleQueue<_Element>::DoubleQueue(DoubleQueue&& _another)
{
	std::scoped_lock lock(_another._exitMutex, \
		_another._entryMutex);
	move(*this, std::forward<DoubleQueue>(_another));
}

template <typename _Element>
auto DoubleQueue<_Element>::operator=(const DoubleQueue& _doubleQueue) \
-> DoubleQueue&
{
	if (&_doubleQueue != this)
	{
		std::scoped_lock lock(this->_exitMutex, this->_entryMutex, \
			_doubleQueue._exitMutex, _doubleQueue._entryMutex);
		copy(*this, _doubleQueue);
	}
	return *this;
}

template <typename _Element>
auto DoubleQueue<_Element>::operator=(DoubleQueue&& _doubleQueue) \
-> DoubleQueue&
{
	if (&_doubleQueue != this)
	{
		std::scoped_lock lock(this->_exitMutex, this->_entryMutex, \
			_doubleQueue._exitMutex, _doubleQueue._entryMutex);
		move(*this, std::forward<DoubleQueue>(_doubleQueue));
	}
	return *this;
}

template <typename _Element>
auto DoubleQueue<_Element>::push(const Element& _element) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (auto capacity = this->capacity(); \
		capacity > 0 && size() >= capacity)
		return std::nullopt;

	_entryQueue.push_back(_element);
	return add(1);
}

template <typename _Element>
auto DoubleQueue<_Element>::push(Element&& _element) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (auto capacity = this->capacity(); \
		capacity > 0 && size() >= capacity)
		return std::nullopt;

	_entryQueue.push_back(std::forward<Element>(_element));
	return add(1);
}

template <typename _Element>
auto DoubleQueue<_Element>::push(QueueType& _queue) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (not valid(_queue)) return std::nullopt;

	auto size = _queue.size();
	_entryQueue.splice(_entryQueue.cend(), _queue);
	return add(size);
}

template <typename _Element>
auto DoubleQueue<_Element>::push(QueueType&& _queue) \
-> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (not valid(_queue)) return std::nullopt;

	auto size = _queue.size();
	_entryQueue.splice(_entryQueue.cend(), \
		std::forward<QueueType>(_queue));
	return add(size);
}

// 支持元素的完全移动语义
template <typename _Element>
bool DoubleQueue<_Element>::pop(Element& _element)
{
	std::lock_guard lock(_exitMutex);
	if (empty()) return false;

	if (_exitQueue.empty())
	{
		std::lock_guard lock(_entryMutex);
		_exitQueue.swap(_entryQueue);
	}

	subtract(1);
	_element = std::move(_exitQueue.front());
	_exitQueue.pop_front();
	return true;
}

// 编译器RVO机制决定完全移动语义或者移动语义与复制语义
template <typename _Element>
auto DoubleQueue<_Element>::pop() -> std::optional<Element>
{
	std::lock_guard lock(_exitMutex);
	if (empty()) return std::nullopt;

	if (_exitQueue.empty())
	{
		std::lock_guard lock(_entryMutex);
		_exitQueue.swap(_entryQueue);
	}

	subtract(1);
	std::optional result = std::move(_exitQueue.front());
	_exitQueue.pop_front();
	return result;
}

template <typename _Element>
bool DoubleQueue<_Element>::pop(QueueType& _queue)
{
	std::lock_guard exitLock(_exitMutex);
	if (empty()) return false;

	_queue.splice(_queue.cend(), _exitQueue);

	std::lock_guard entryLock(_entryMutex);
	_queue.splice(_queue.cend(), _entryQueue);
	set(_size, 0);
	return true;
}

template <typename _Element>
auto DoubleQueue<_Element>::clear() -> SizeType
{
	std::scoped_lock lock(_exitMutex, _entryMutex);
	_exitQueue.clear();
	_entryQueue.clear();
	return exchange(_size, 0);
}

ETERFREE_SPACE_END
