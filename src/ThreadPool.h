/*
Eterfree Classes Library For C++ 17

Copyright @2017 CDU Innovation Studio
All rights reserved.

文件名称：ThreadPool.h
摘要：
1.此头文件声明线程池类，在ThreadPool.cpp中实现。
2.当任务队列为空时，管理线程进入阻塞状态，向任务队列增加任务时，唤醒阻塞的管理线程，为工作线程分配任务。
3.当无空闲工作线程时，管理线程进入阻塞状态，存在空闲工作线程时，唤醒阻塞的管理线程，为空闲工作线程分配任务。
4.任务队列包含任务函数子和回调函数子，同时传递给工作线程，工作线程执行任务函数子之后调用回调函数子。

当前版本：V1.3
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2019年03月08日

修正日志：
V1.1
1.线程设置空闲线程表，各工作线程主动调用getTask获取任务时，若任务队列为空，则把对应的工作线程加入空闲线程表，由管理线程分配任务时，遍历空闲线程表，唤醒空闲线程
2.智能增减工作线程方案：线程池管理线程根据任务队列中任务数量级，改变当前工作线程数量
	根据任务数量，适当增加或者减少工作线程，增加工作线程数量，实现高并发执行任务，释放空闲工作线程，提高资源利用率
V1.2
1.增加空闲工作线程计数，删除空闲线程表
2.修复销毁线程池时死锁隐患
V1.3
1.以移动语义优化获取任务，减少不必要的复制步骤
2.去除分配任务的多余判断步骤
3.使用双缓冲任务队列，减少读写任务之间的影响，提高放入任务和获取任务的效率
*/

#pragma once

#include <list>
#include <memory>
#include <functional>

//#define DEFAULT_TIME_SLICE 2

class Thread;
struct ThreadPoolStructure;
using TaskPair = std::pair<std::function<void()>, std::function<void()>>;

class ThreadPool
{
	friend class Thread;
	std::unique_ptr<ThreadPoolStructure> threadPoolData;
	inline void setCloseStatus(bool bClosed);
	inline bool getCloseStatus() const;
public:
	ThreadPool(unsigned nThread = 0, unsigned maxThreads = getMaxConcurrency() * 100);
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool operator= (const ThreadPool&) = delete;
	~ThreadPool();
	static unsigned getMaxConcurrency();
	bool setCurrentThreads(unsigned nThread);
	unsigned getCurrentThreads() const;
	unsigned getTasks() const;
	void setMaxThreads(unsigned maxThreads);
	unsigned getMaxThreads() const;
	//bool setTimeSlice(unsigned timeSlice);
	//unsigned getTimeSlice() const;
	void pushTask(std::function<void()> run, std::function<void()> callback);
	void pushTask(TaskPair &&task);
	void pushTask(std::list<TaskPair> &tasks);
	void destroy();
protected:
	bool getTask(std::shared_ptr<Thread> thread);
	void execute();
};
