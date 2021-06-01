/*
文件名称：Queue.h
摘要：
1.定义双缓冲队列类模板Queue。
2.双缓冲队列包括入口队列和出口队列，以交换策略降低二者的相互影响。

版本：v1.4
作者：许聪
邮箱：2592419242@qq.com
创建日期：2019年03月08日
更新日期：2021年06月01日

变化：
v1.1
1.在放入元素之时，只锁定入口互斥元。在取出元素之时，先锁定出口互斥元，若出口队列为空，再锁定入口互斥元，并且交换两个队列。
	以此降低两个队列的相互影响，从而提高出入队列的效率。
v1.2
1.调整命名风格。
v1.3
1.归入名称空间eterfree。
v1.4
1.自定义队列容量。
2.弃用非线程安全函数front和pop，启用线程安全函数pop。
	在调用非线程安全函数front和pop之前，需要调用函数mutex获取出口互斥元。
	于是提供线程安全函数pop，替换以前繁琐的调用步骤。
*/

#pragma once

#include <optional>
#include <list>
#include <atomic>
#include <mutex>

#include "core.h"

ETERFREE_BEGIN

template <typename _DataType>
class Queue
{
public:
	using DataType = _DataType;
	using QueueType = std::list<DataType>;
	using SizeType = typename QueueType::size_type;
	using MutexType = std::mutex;

private:
	SizeType _capacity;
	std::atomic<SizeType> _size;
	MutexType _entryMutex;
	MutexType _exitMutex;
	QueueType _entryQueue;
	QueueType _exitQueue;

public:
	// 若_capacity小于等于零，则无限制，否则为上限值
	Queue(SizeType _capacity = 0) : _capacity(_capacity), _size(0) {}

	SizeType size() const noexcept { return _size.load(std::memory_order::memory_order_relaxed); }
	bool empty() const noexcept { return size() == 0; }
	MutexType& mutex() noexcept { return _exitMutex; }

	std::optional<SizeType> push(DataType&& _data);
	std::optional<SizeType> push(QueueType& _data);

	DEPRECATED
	DataType& front();
	DEPRECATED
	void pop() noexcept;

	bool pop(DataType& _data);
	//std::optional<DataType> pop();
};

template <typename _DataType>
std::optional<typename Queue<_DataType>::SizeType> Queue<_DataType>::push(DataType&& _data)
{
	std::lock_guard lock(_entryMutex);
	if (_capacity > 0 && size() >= _capacity)
		return std::nullopt;

	_entryQueue.push_back(std::forward<DataType>(_data));
	return _size.fetch_add(1, std::memory_order::memory_order_relaxed);
}

template <typename _DataType>
std::optional<typename Queue<_DataType>::SizeType> Queue<_DataType>::push(QueueType& _data)
{
	std::lock_guard lock(_entryMutex);
	auto quantity = _data.size();
	if (auto size = this->size(); \
		_capacity > 0 && (size >= _capacity || quantity >= _capacity - size))
		return std::nullopt;

	_entryQueue.splice(_entryQueue.cend(), _data);
	return _size.fetch_add(quantity, std::memory_order::memory_order_relaxed);
}

template <typename _DataType>
typename Queue<_DataType>::DataType& Queue<_DataType>::front()
{
	if (_exitQueue.empty())
	{
		_entryMutex.lock();
		_exitQueue.swap(_entryQueue);
		_entryMutex.unlock();
	}
	return _exitQueue.front();
}

template <typename _DataType>
void Queue<_DataType>::pop() noexcept
{
	_size.fetch_sub(1, std::memory_order::memory_order_relaxed);
	_exitQueue.pop_front();
}

template <typename _DataType>
bool Queue<_DataType>::pop(DataType& _data)
{
	std::lock_guard lock(_exitMutex);
	if (empty())
		return false;

	if (_exitQueue.empty())
	{
		_entryMutex.lock();
		_exitQueue.swap(_entryQueue);
		_entryMutex.unlock();
	}

	_data = _exitQueue.front();
	_size.fetch_sub(1, std::memory_order::memory_order_relaxed);
	_exitQueue.pop_front();
	return true;
}

//template <typename _DataType>
//std::optional<typename Queue<_DataType>::DataType> Queue<_DataType>::pop()
//{
//	std::lock_guard locker(_exitMutex);
//	if (empty())
//		return std::nullopt;
//
//	if (_exitQueue.empty())
//	{
//		_entryMutex.lock();
//		_exitQueue.swap(_entryQueue);
//		_entryMutex.unlock();
//	}
//
//	std::optional data = _exitQueue.front();
//	_size.fetch_sub(1, std::memory_order::memory_order_relaxed);
//	_exitQueue.pop_front();
//	return data;
//}

ETERFREE_END
