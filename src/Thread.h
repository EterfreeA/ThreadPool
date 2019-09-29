/*
Eterfree Classes Library For C++ 17

Copyright @2017 CDU Innovation Studio
All rights reserved.

文件名称：Thread.h
摘要：
1.此头文件声明线程类Thread，实现于Thread.cpp。
2.Thread类提供线程重用方案，创建的线程可以被反复使用。
3.线程在创建后立即进入阻塞状态，调用configure函数给线程分配任务，调用start函数启动线程。
4.执行任务函数子之后，立即调用回调函数子。
5.于调用函数子过程捕获异常，保证线程正常执行，防止线程泄漏。

当前版本：V1.4
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2019年09月29日

修正日志：
V1.1
1.线程执行任务之后，自动于任务队列获取任务，若获取任务失败，进入阻塞状态，等待守护线程唤醒并分配任务，否则执行新任务，从而提高线程效率
V1.2
1.简化构造函数，取消任务参数，精简代码
2.以移动语义优化配置任务操作，减少不必要的复制步骤
V1.3
1.调整命名风格，优化类接口
2.清除多余线程状态，合并介入标志（即允许分配任务）至运行状态，使线程状态转换逻辑清晰
3.换回复制语义配置任务，任务形式乃函数子对，存放数据少，复制操作对效率影响小
	此方案支持任务来源存留备份，避免线程异常而丢失任务
4.新加移动语义构造和赋值，将线程主函数声明为静态成员函数，除去与类成员指针this的关联性
5.支持选择性配置任务队列和回调函数子，分别用于自动获取任务和回调通知线程阻塞状态
V1.4
1.优化成员函数访问权限
2.归入名称空间eterfree
3.类Thread内外分别声明和定义结构体ThreadStructure，避免污染类外名称空间，从而增强类的封装性
*/

#pragma once

#include <thread>
#include <memory>
#include <functional>

#include "Queue.h"

ETERFREE_BEGIN

/* 继承enable_shared_from_this模板类，当Thread被shared_ptr托管，
而在Thread把类成员指针this作为参数传给其他函数时，
需要传递this的shared_ptr，调用shared_from_this函数获取this的shared_ptr。
不可以直接传递原始指针this，否则不能保证shared_ptr的语义，也许会导致已被释放的错误。
也不可以再创建另一shared_ptr，否则托管的多个shared_ptr的控制块不同，导致同一对象被释放多次。 */
class Thread
	//: public std::enable_shared_from_this<Thread>
{
	struct ThreadStructure;
	using data_type = std::shared_ptr<ThreadStructure>;
	data_type data;
public:
	using functor = std::function<void()>;
	using TaskPair = std::pair<functor, functor>;
	using ThreadID = std::thread::id;
	Thread();
	Thread(const Thread&) = delete;
	Thread(Thread&&) = default;
	~Thread();
	Thread& operator=(const Thread&) = delete;
	Thread& operator=(Thread&&) = default;
	bool configure(std::shared_ptr<Queue<TaskPair>> taskQueue,
		std::function<void(bool, ThreadID)> callback);
	bool configure(const TaskPair& task);
	bool start();
	ThreadID getThreadID() const;
	bool free() const;
	//const void *getParameters();
private:
	static void setClosed(data_type& data, bool closed);
	static bool getClosed(const data_type& data);
	static void setRunning(data_type& data, bool running);
	static bool getRunning(const data_type& data);
	void destroy();
	static void execute(data_type data);
	//virtual void process();
	//virtual void callback();
};

ETERFREE_END
