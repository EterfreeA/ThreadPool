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

constexpr auto DEFAULT_UPPER_LIMIT = 10000U;

template <typename Type>
class Queue
{
public:
	using size_type = typename std::list<Type>::size_type;
	Queue(size_type upperLimit = DEFAULT_UPPER_LIMIT);
	~Queue();
	const std::atomic<size_type>& size() const { return counter; }
	std::mutex& mutex() { return readMutex; }
	bool empty() const { return counter == 0; }
	bool push(Type&& data);
	bool push(std::list<Type>& data);
	Type& front();
	void pop();
protected:
	//size_type upperLimit;
	std::atomic<size_type> counter = 0;
	std::mutex writeMutex;
	std::mutex readMutex;
	std::list<Type> writeQueue;
	std::list<Type> readQueue;
};

template <typename Type>
Queue<Type>::Queue(size_type upperLimit)
{
	//if (upperLimit > 0)
	//	this->upperLimit = upperLimit;
}

template <typename Type>
Queue<Type>::~Queue()
{
	
}

template <typename Type>
bool Queue<Type>::push(Type&& data)
{
	std::lock_guard<std::mutex> writeLocker(writeMutex);
	//if (writeQueue.size() >= upperLimit)
	//	return false;
	writeQueue.push_back(std::forward<Type&&>(data));
	++counter;
	return true;
}

template <typename Type>
bool Queue<Type>::push(std::list<Type>& data)
{
	std::lock_guard<std::mutex> writeLocker(writeMutex);
	auto quantity = data.size();
	//if (writeQueue.size() + quantity >= upperLimit)
	//	return false;
	writeQueue.splice(writeQueue.cend(), data);
	counter += quantity;
	return true;
}

template <typename Type>
Type& Queue<Type>::front()
{
	if (readQueue.empty())
	{
		writeMutex.lock();
		readQueue.swap(writeQueue);
		writeMutex.unlock();
	}
	return readQueue.front();
}

template <typename Type>
void Queue<Type>::pop()
{
	--counter;
	readQueue.pop_front();
}

ETERFREE_END
