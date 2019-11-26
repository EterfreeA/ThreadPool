/*
Eterfree Classes Library For C++ 17

Copyright @2017 CDU Innovation Studio
All rights reserved.

文件名称：Queue.h
摘要：
1.此头文件声明双缓冲队列类模板。
2.双缓冲队列包括写入队列和读取队列，两个队列的操作均为加锁操作，并且有不同锁，因此接口线程安全。

当前版本：V1.3
作者：许聪
邮箱: 2592419242@qq.com
创建日期：2019年03月08日
更新日期：2019年09月27日

修正日志：
V1.1
1.在写入队列之时，只获取写队列锁。在读取队列之时，先获取读队列锁，若读取队列为空，再获取写入队列锁，并且交换两个队列。
	从而减少读写之间的影响，提高读写效率
V1.2
1.调整命名风格
V1.3
1.归入名称空间eterfree
*/

#pragma once

#include <list>
#include <mutex>
#include <atomic>

#include "core.h"

ETERFREE_BEGIN

template <typename DataType>
class Queue
{
public:
	using QueueType = std::list<DataType>;
	using SizeType = typename QueueType::size_type;
	using CountType = std::atomic<SizeType>;
	using Mutex = std::mutex;
	static constexpr auto DEFAULT_UPPER_LIMIT = 10000U;
	Queue(SizeType upperLimit = DEFAULT_UPPER_LIMIT);
	~Queue();
	const CountType& size() const { return counter; }
	Mutex& mutex() { return readMutex; }
	bool empty() const { return counter == 0; }
	bool push(DataType&& data);
	bool push(QueueType& data);
	DataType& front();
	void pop();
protected:
	//SizeType upperLimit;
	CountType counter = 0;
	Mutex writeMutex;
	Mutex readMutex;
	QueueType writeQueue;
	QueueType readQueue;
};

template <typename DataType>
Queue<DataType>::Queue(SizeType upperLimit)
{
	//if (upperLimit > 0U)
	//	this->upperLimit = upperLimit;
}

template <typename DataType>
Queue<DataType>::~Queue()
{
	
}

template <typename DataType>
bool Queue<DataType>::push(DataType&& data)
{
	std::lock_guard writeLocker(writeMutex);
	//if (writeQueue.size() >= upperLimit)
	//	return false;
	writeQueue.push_back(std::forward<DataType&&>(data));
	++counter;
	return true;
}

template <typename DataType>
bool Queue<DataType>::push(QueueType& data)
{
	std::lock_guard writeLocker(writeMutex);
	auto quantity = data.size();
	//if (writeQueue.size() + quantity >= upperLimit)
	//	return false;
	writeQueue.splice(writeQueue.cend(), data);
	counter += quantity;
	return true;
}

template <typename DataType>
DataType& Queue<DataType>::front()
{
	if (readQueue.empty())
	{
		writeMutex.lock();
		readQueue.swap(writeQueue);
		writeMutex.unlock();
	}
	return readQueue.front();
}

template <typename DataType>
void Queue<DataType>::pop()
{
	--counter;
	readQueue.pop_front();
}

ETERFREE_END
