/*
Eterfree Classes Library For C++ 17

Copyright @2017 CDU Innovation Studio
All rights reserved.

文件名称：Thread.h
摘要：
1.此头文件声明工作线程类，在Thread.cpp中实现。
2.Thread类提供线程重用方案，创建的线程可以被重复使用。
3.创建线程之后，立即进入阻塞状态，调用configure函数为线程分配任务，调用start函数来启动线程。
4.如果任务函数子为空，调用空函数体run（run是虚函数，可被重写），若不为空，调用任务函数子。执行任务函数子之后，调用回调函数子。
5.在调用函数子的过程捕获异常，保证线程正常执行，不至于线程泄漏。

当前版本：V1.2
作者：许聪
邮箱：2592419242@qq.com
创建日期：2017年09月22日
更新日期：2019年03月08日

修正日志：
V1.1
1.工作线程执行当前任务之后，自动获取任务队列之中的任务，若获取任务失败，进入阻塞状态，等待管理线程唤醒并分配任务，否则执行获取的新任务，从而提高工作线程的效率
V1.2
1.简化构造函数，取消任务接口，精简代码
2.以移动语义优化配置任务，减少不必要的复制步骤
*/

#pragma once

#include <thread>
#include <memory>
#include <functional>

class ThreadPool;
struct ThreadStructure;
using TaskPair = std::pair<std::function<void()>, std::function<void()>>;

/* 继承enable_shared_from_this模板类，当Thread被shared_ptr托管，
在Thread类中把指向类对象自身的指针this作为参数传给其他函数时，
需要传递指向自身的shared_ptr，调用shared_from_this函数获取指向自身的shared_ptr。
不可以直接传递原始指针this，否则不能保证shared_ptr的语义，也许会导致已被释放的错误。
也不可以再创建另一shared_ptr，否则托管的多个shared_ptr的控制块不同，导致同一对象被释放多次。 */
class Thread :public std::enable_shared_from_this<Thread>
{
	std::shared_ptr<ThreadStructure> threadData;
	inline void setCloseStatus(bool bClosed);
	inline bool getCloseStatus() const;
	inline void setRunStatus(bool bRunning);
	inline bool getRunStatus() const;
	inline void setInterveneStatus(bool bIntervening);
	inline bool getInterveneStatus() const;
public:
	Thread(ThreadPool *threadPool);
	Thread(const Thread&) = delete;
	Thread operator= (const Thread&) = delete;
	~Thread();
	bool configure(TaskPair &&task);
	bool start();
	void destroy();
	std::thread::id getThreadId() const;
	//const void *getParameters();
	bool isFree() const;
protected:
	void execute();
	virtual void run();
	virtual void callback();
};
