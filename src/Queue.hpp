/*
* 文件名称：Queue.hpp
* 语言标准：C++20
* 
* 创建日期：2019年03月08日
* 更新日期：2022年03月20日
* 
* 摘要
* 1.定义双缓冲队列类模板Queue。
* 2.支持自定义队列容量，包含入口队列和出口队列，并以交换策略降低二者的相互影响。
* 3.在放入元素之时，只锁定入口互斥元。在取出元素之时，先锁定出口互斥元，若出口队列为空，再锁定入口互斥元，并且交换两个队列。
*   以此降低两个队列的相互影响，从而提高出入队列的效率。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.5.2
* 变化
* v1.5.1
* 1.入队列可选复制语义或者移动语义。
* 2.支持批量出队列。
* 3.新增清空队列方法。
* v1.5.2
* 1.支持动态调整容量。
*/

#pragma once

#include <utility>
#include <optional>
#include <list>
#include <atomic>
#include <mutex>

#include "Core.hpp"

ETERFREE_SPACE_BEGIN

template <typename _ElementType>
class Queue
{
public:
	using ElementType = _ElementType;
	using QueueType = std::list<ElementType>;
	using SizeType = QueueType::size_type;

private:
	using AtomicType = std::atomic<SizeType>;
	using MutexType = std::mutex;

private:
	AtomicType _capacity;
	AtomicType _size;

	MutexType _entryMutex;
	QueueType _entryQueue;

	MutexType _exitMutex;
	QueueType _exitQueue;

private:
	static void set(AtomicType& _atomic, SizeType _size) noexcept
	{
		_atomic.store(_size, std::memory_order::relaxed);
	}

	static auto get(const AtomicType& _atomic) noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

private:
	auto add(SizeType _size) noexcept
	{
		return this->_size.fetch_add(_size, std::memory_order::relaxed);
	}

	auto subtract(SizeType _size) noexcept
	{
		return this->_size.fetch_sub(_size, std::memory_order::relaxed);
	}

public:
	// 若_capacity小于等于零，则无限制，否则其为上限值
	Queue(SizeType _capacity = 0)
		: _capacity(_capacity), _size(0) {}

	auto capacity() const noexcept { return get(_capacity); }
	void reserve(SizeType _capacity) noexcept
	{
		set(this->_capacity, _capacity);
	}

	auto size() const noexcept { return get(_size); }
	bool empty() const noexcept { return size() == 0; }

	std::optional<SizeType> push(const ElementType& _element);
	std::optional<SizeType> push(ElementType&& _element);

	std::optional<SizeType> push(QueueType& _queue);
	std::optional<SizeType> push(QueueType&& _queue);

	bool pop(ElementType& _element);
	std::optional<ElementType> pop();

	bool pop(QueueType& _queue);

	void clear();
};

template <typename _ElementType>
auto Queue<_ElementType>::push(const ElementType& _element) -> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (auto capacity = this->capacity(); \
		capacity > 0 and size() >= capacity)
		return std::nullopt;

	_entryQueue.push_back(_element);
	return add(1);
}

template <typename _ElementType>
auto Queue<_ElementType>::push(ElementType&& _element) -> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (auto capacity = this->capacity(); \
		capacity > 0 and size() >= capacity)
		return std::nullopt;

	_entryQueue.push_back(std::forward<ElementType>(_element));
	return add(1);
}

template <typename _ElementType>
auto Queue<_ElementType>::push(QueueType& _queue) -> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (auto capacity = this->capacity(), size = this->size(); \
		capacity > 0 and (size >= capacity or _queue.size() >= capacity - size))
		return std::nullopt;

	auto size = _queue.size();
	_entryQueue.splice(_entryQueue.cend(), _queue);
	return add(size);
}

template <typename _ElementType>
auto Queue<_ElementType>::push(QueueType&& _queue) -> std::optional<SizeType>
{
	std::lock_guard lock(_entryMutex);
	if (auto capacity = this->capacity(), size = this->size(); \
		capacity > 0 and (size >= capacity or _queue.size() >= capacity - size))
		return std::nullopt;

	auto size = _queue.size();
	_entryQueue.splice(_entryQueue.cend(), std::forward<QueueType>(_queue));
	return add(size);
}

template <typename _ElementType>
bool Queue<_ElementType>::pop(ElementType& _element)
{
	std::lock_guard lock(_exitMutex);
	if (empty()) return false;

	if (_exitQueue.empty())
	{
		std::lock_guard lock(_entryMutex);
		_exitQueue.swap(_entryQueue);
	}

	subtract(1);
	_element = _exitQueue.front();
	_exitQueue.pop_front();
	return true;
}

template <typename _ElementType>
auto Queue<_ElementType>::pop() -> std::optional<ElementType>
{
	std::lock_guard lock(_exitMutex);
	if (empty()) return std::nullopt;

	if (_exitQueue.empty())
	{
		std::lock_guard lock(_entryMutex);
		_exitQueue.swap(_entryQueue);
	}

	subtract(1);
	std::optional result = _exitQueue.front();
	_exitQueue.pop_front();
	return result;
}

template <typename _ElementType>
bool Queue<_ElementType>::pop(QueueType& _queue)
{
	std::lock_guard exitLock(_exitMutex);
	if (empty()) return false;

	_queue.splice(_queue.cend(), _exitQueue);

	std::lock_guard entryLock(_entryMutex);
	_queue.splice(_queue.cend(), _entryQueue);
	set(_size, 0);
	return true;
}

template <typename _ElementType>
void Queue<_ElementType>::clear()
{
	std::scoped_lock lock(_exitMutex, _entryMutex);
	_exitQueue.clear();
	_entryQueue.clear();
	set(_size, 0);
}

ETERFREE_SPACE_END
