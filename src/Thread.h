﻿/*
文件名称：Thread.h
摘要：
1.线程类Thread定义于此文件，实现于Thread.cpp。
2.Thread类提供线程重用方案，线程可以反复使用。
3.线程在创建之后进入阻塞状态，调用configure函数分配任务，调用start函数启动线程。
4.于执行函数子时捕获异常，保证线程正常执行任务，防止线程泄漏。

版本：v1.6
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2019年10月17日

日志：
v1.1
1.线程执行任务之后，自动于任务队列获取任务。
	若获取任务失败，进入阻塞状态，等待守护线程分配任务并唤醒之，否则执行新任务；从而提高执行效率。
v1.2
1.精简代码，取消构造函数的任务参数。
2.以移动语义优化任务配置操作，减少不必要的复制步骤。
v1.3
1.调整命名风格，优化类接口。
2.清除多余线程状态，合并介入标志（即允许分配任务）至运行标志，使线程状态转换逻辑清晰。
3.任务配置放弃使用移动语义，换回复制语义，任务形式乃函数子，存放数据少，复制操作对效率影响小。
	此方案支持任务来源留存备份，避免线程异常而丢失任务。
4.新增移动语义构造和赋值线程，工作线程主函数声明为静态成员，除去与类成员指针this的关联性。
5.支持选择性配置任务队列和回调函数子。
	分别用于自动获取任务，以及回调通知线程池获取任务失败，线程进入阻塞状态。
v1.4
1.优化成员函数访问权限。
2.归入名称空间eterfree。
3.类Thread内外分别声明和定义结构体Structure，避免污染类外名称空间，从而增强类的封装性。
v1.5
1.优化构造函数，以构造初始化成员变量，去掉多余赋值步骤。
2.精简任务形式为函数子，不再含有回调函数子，若执行任务需要回调，可以在任务函数子之内进行回调步骤。
v1.6
1.优化虚假唤醒解决方案，于启动方法判断任务是否为空，取消条件变量成立判别式。
2.尝试解决唤醒先于阻塞的隐患，在等待条件变量之前，若满足执行条件就不必等待。
3.工作线程退出通道增设条件，保证在退出之前，执行已配置任务。
4.于头文件移动Queue.h的引用至源文件，以双缓冲队列模板的声明式替换其定义式，除去Thread.h对Queue.h的编译依存性。
*/

#pragma once

#include <functional>
#include <memory>
#include <thread>

#include "core.h"

ETERFREE_BEGIN

template <typename DataType>
class Queue;

/* 继承enable_shared_from_this模板类，当Thread被shared_ptr托管，
而在Thread把类成员指针this作为参数传给其他函数时，
需要传递this的shared_ptr，调用shared_from_this函数获取this的shared_ptr。
不可以直接传递原始指针this，否则不能保证shared_ptr的语义，也许会导致已被释放的错误。
也不可以再创建另一shared_ptr，否则托管的多个shared_ptr的控制块不同，导致同一对象被释放多次。 */
class Thread
	//: public std::enable_shared_from_this<Thread>
{
	struct Structure;
	using DataType = std::shared_ptr<Structure>;
	DataType data;
public:
	using Functor = std::function<void()>;
	using ThreadID = std::thread::id;
	Thread();
	Thread(const Thread&) = delete;
	Thread(Thread&&) = default;
	~Thread();
	Thread& operator=(const Thread&) = delete;
	Thread& operator=(Thread&&) = default;
	bool configure(std::shared_ptr<Queue<Functor>> taskQueue, \
		std::function<void(bool, ThreadID)> callback);
	bool configure(const Functor& task);
	bool start();
	ThreadID getThreadID() const;
	bool free() const;
	//const void *getParameters();
private:
	static void setClosed(DataType& data, bool closed);
	static bool getClosed(const DataType& data);
	static void setRunning(DataType& data, bool running);
	static bool getRunning(const DataType& data);
	static void execute(DataType data);
	//virtual void process();
	void destroy();
};

ETERFREE_END
