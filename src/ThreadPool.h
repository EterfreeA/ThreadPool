/*
Eterfree Classes Library For C++ 17

Copyright @2017 CDU Innovation Studio
All rights reserved.

文件名称：ThreadPool.h
摘要：
1.线程池类ThreadPool定义于此文件，实现于ThreadPool.cpp。
2.当任务队列为空时，守护线程进入阻塞状态，向任务队列增加任务时，唤醒阻塞的守护线程，为线程分配任务。
3.当无空闲线程时，守护线程进入阻塞状态，存在空闲线程时，唤醒阻塞的守护线程，为空闲线程分配任务。

当前版本：V1.7
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2019年10月16日

修正日志：
V1.1
1.设置空闲线程表，线程主动获取任务时，若任务队列为空，将对应线程加入空闲线程表，而守护线程分配任务时，遍历空闲线程表，唤醒空闲线程。
V1.2
1.增加空闲线程计数，删除空闲线程表。
2.修复销毁线程池的死锁隐患。
V1.3
1.以移动语义优化任务获取操作，减少不必要的复制步骤。
2.除去分配任务的多余判断步骤。
3.使用双缓冲任务队列，减少读写任务之间的影响，提高放入任务和取出任务的效率。
V1.4
1.调整命名风格，优化类接口。
2.优化任务分配逻辑。
3.支持非阻塞式销毁线程池。
	调用销毁函数，设为关闭状态并唤醒守护线程，由守护线程销毁线程。
4.新增移动语义构造和赋值线程池，守护线程主函数声明为静态成员，除去与类成员指针this的关联性。
V1.5
1.优化成员函数访问权限。
2.归入名称空间eterfree。
3.运用析构函数自动销毁线程。
4.以weak_ptr解决shared_ptr循环引用问题，防止销毁线程池时内存泄漏。
5.类ThreadPool内外分别声明和定义结构体ThreadPoolStructure，避免污染类外名称空间，从而增强类的封装性。
V1.6
1.优化构造函数，以构造初始化成员变量，去掉多余赋值步骤。
2.精简任务形式为函数子，不再含有回调函数子，以降低内存消耗。
V1.7
1.添加任务时过滤空任务，增强线程池的健壮性，防止给线程配置任务成功而启动线程失败。
	空任务会导致守护线程失去启动线程的能力，在最坏的情况，所有线程处于阻塞状态，线程池无法处理任务，任务堆积过多，耗尽内存，最终程序崩溃。
2.优化条件变量，去掉多余判断步骤。
V100
1.智能增减线程方案：线程池守护线程根据任务队列的任务数量级，改变当前线程数量。
	根据任务数量，适当增加或者减少线程，增加线程数量，实现高并发执行任务，释放空闲线程，提高资源利用率。
*/

#pragma once

#include <memory>
#include <list>
#include <functional>

#include "core.h"

ETERFREE_BEGIN
//#define DEFAULT_TIME_SLICE 2

class ThreadPool
{
	//friend class Thread;
	struct ThreadPoolStructure;
	using data_type = std::shared_ptr<ThreadPoolStructure>;
	data_type data;
public:
	using size_type = std::size_t;
	using functor = std::function<void()>;
	ThreadPool(size_type threads = 0, size_type maxThreads = getConcurrency() * 100);
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = default;
	~ThreadPool();
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool& operator=(ThreadPool&&) = default;
	static size_type getConcurrency();
	//bool setTimeSlice(size_type timeSlice);
	//size_type getTimeSlice() const;
	void setMaxThreads(size_type maxThreads);
	size_type getMaxThreads() const;
	bool setThreads(size_type threads);
	size_type getThreads() const;
	size_type getFreeThreads() const;
	size_type getTasks() const;
	void pushTask(functor&& task);
	void pushTask(std::list<functor>& tasks);
private:
	static void setClosed(data_type& data, bool closed);
	static bool getClosed(const data_type& data);
	static void execute(data_type data);
	//bool getTask(std::shared_ptr<Thread> thread);
	void destroy();
};

ETERFREE_END
