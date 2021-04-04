/*
文件名称：Queue.hpp
摘要：
1.定义双缓冲队列类模板Queue。
2.支持自定义队列容量，包含读队列和写队列，并以交换策略降低二者的相互影响。
3.在写入数据之时，只锁定写互斥元。在读出数据之时，先锁定读互斥元，若读队列为空，再锁定写互斥元，并交换两个队列。
	以此降低读写的相互影响，从而提高读写效率。

版本：v1.5.0
作者：许聪
邮箱：2592419242@qq.com
创建日期：2019年03月08日
更新日期：2021年04月04日
*/

#pragma once

#include <optional>
#include <list>
#include <atomic>
#include <mutex>

#include "Core.hpp"

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
	MutexType _readMutex;
	MutexType _writeMutex;
	QueueType _readQueue;
	QueueType _writeQueue;

public:
	// 若_capacity小于等于零，则无限制，否则为上限值
	Queue(SizeType _capacity = 0) : _capacity(_capacity), _size(0) {}

	SizeType size() const noexcept { return _size.load(std::memory_order::memory_order_relaxed); }
	bool empty() const noexcept { return size() == 0; }
	MutexType& mutex() noexcept { return _readMutex; }

	std::optional<SizeType> push(DataType&& _data);
	std::optional<SizeType> push(QueueType& _data);

	std::optional<DataType> pop();
};

template <typename _DataType>
std::optional<typename Queue<_DataType>::SizeType> Queue<_DataType>::push(DataType&& _data)
{
	std::lock_guard writeLocker(_writeMutex);
	if (_capacity > 0 && size() >= _capacity)
		return std::nullopt;

	_writeQueue.push_back(std::forward<DataType>(_data));
	return _size.fetch_add(1, std::memory_order::memory_order_relaxed);
}

template <typename _DataType>
std::optional<typename Queue<_DataType>::SizeType> Queue<_DataType>::push(QueueType& _data)
{
	std::lock_guard writeLocker(_writeMutex);
	auto quantity = _data.size();
	if (auto size = this->size(); \
		_capacity > 0 && (size >= _capacity || quantity >= _capacity - size))
		return std::nullopt;

	_writeQueue.splice(_writeQueue.cend(), _data);
	return _size.fetch_add(quantity, std::memory_order::memory_order_relaxed);
}

template <typename _DataType>
std::optional<typename Queue<_DataType>::DataType> Queue<_DataType>::pop()
{
	std::lock_guard lock(_readMutex);
	if (empty())
		return std::nullopt;

	if (_readQueue.empty())
	{
		_writeMutex.lock();
		_readQueue.swap(_writeQueue);
		_writeMutex.unlock();
	}

	std::optional data = _readQueue.front();
	_size.fetch_sub(1, std::memory_order::memory_order_relaxed);
	_readQueue.pop_front();
	return data;
}

ETERFREE_END
