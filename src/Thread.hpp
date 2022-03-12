/*
* 文件名称：Thread.hpp
* 语言标准：C++20
* 
* 创建日期：2017年09月22日
* 更新日期：2022年03月13日
* 
* 摘要
* 1. 定义线程类模板Thread。
* 2. Thread提供线程重用方案，支持销毁再创建，一次创建反复使用。
* 3. 线程在创建之后进入阻塞状态，先调用函数configure分配任务，再调用函数notify激活线程。
* 4. 可选配置单任务或者任务队列，以及回调函数子。
*   任务队列用于自动获取任务，回调函数子用于通知线程池，线程执行完单个任务，以及当前闲置状态。
* 5. 执行任务之时捕获异常，防止线程泄漏。
* 6. 线程执行任务之后，倘若配置有任务队列，主动获取任务，否则进入阻塞状态。
*   倘若获取任务失败，等待分配任务；否则执行新任务，从而提高执行效率。
* 7. 线程在退出之前，倘若配置有任务队列，确保完成所有任务，否则仅执行配置任务。
* 8. 以原子操作确保接口的线程安全性，以单状态枚举确保接口的执行顺序。
* 9. 引入条件类模板Condition，当激活先于阻塞之时，确保线程正常退出。
* 10.线程主函数声明为静态成员，除去与类成员指针this的关联性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v2.0.3
* 变化
* v2.0.1
* 1.运用Condition的宽松策略，提升激活线程的效率。
* v2.0.2
* 1.消除谓词对条件实例有效性的重复判断。
* v2.0.3
* 1.以原子操作确保移动语义的线程安全性。
* 2.解决配置先于回调隐患。
*/

#pragma once

#include <functional>
#include <utility>
#include <source_location>
#include <memory>
#include <exception>
#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>

#include "Core.hpp"
#include "Condition.hpp"

ETERFREE_SPACE_BEGIN

template <typename _Element>
class Queue;

/*
继承类模板enable_shared_from_this，当Thread被shared_ptr托管，而需要传递this给其它函数之时，
需要传递指向this的shared_ptr，调用this->shared_from_this获取指向this的shared_ptr。
不可直接传递裸指针this，否则无法确保shared_ptr的语义，也许会导致已被释放的错误。
不可单独创建另一shared_ptr，否则多个shared_ptr的控制块不同，导致释放多次同一对象。
*/
template <typename _Functor = std::function<void()>, typename _Queue = Queue<_Functor>>
class Thread
	//: public std::enable_shared_from_this<Thread>
{
	// 状态枚举
	enum class State
	{
		EMPTY,		// 空态
		INITIAL,	// 初始态
		RUNNABLE,	// 就绪态
		RUNNING,	// 运行态
		BLOCKED,	// 阻塞态
	};

	using Condition = Condition<>;

public:
	using Functor = _Functor;
	using Queue = std::shared_ptr<_Queue>;
	using ThreadID = std::thread::id;
	using Callback = std::function<void(bool, ThreadID)>;

private:
	// 线程数据结构体
	struct Structure
	{
		std::thread _thread;			// 线程实体
		std::atomic<State> _state;		// 原子状态
		std::mutex _threadMutex;		// 线程互斥元
		Condition _condition;			// 强化条件变量

		Queue _taskQueue;				// 任务队列
		Functor _task;					// 任务函数子
		mutable std::mutex _taskMutex;	// 任务互斥元
		Callback _callback;				// 回调函数子

		Structure() : _state(State::EMPTY) {}

		// 获取线程唯一标识
		auto getID() const noexcept { return _thread.get_id(); }

		// 设置状态
		void setState(State _state) noexcept
		{
			this->_state.store(_state, std::memory_order::relaxed);
		}

		// 获取状态
		auto getState() const noexcept
		{
			return _state.load(std::memory_order::relaxed);
		}

		// 设置任务
		void setTask(const decltype(_task)& _task)
		{
			std::lock_guard lock(_taskMutex);
			this->_task = _task;
		}
		void setTask(decltype(_task)&& _task)
		{
			std::lock_guard lock(_taskMutex);
			this->_task = std::move(_task);
		}

		// 任务有效性
		bool getValidity() const
		{
			std::lock_guard lock(_taskMutex);
			return static_cast<bool>(_task);
		}
	};
	using DataType = std::shared_ptr<Structure>;
	using AtomicType = std::atomic<DataType>;

private:
	AtomicType _atomic;

private:
	// 获取任务
	static bool setTask(DataType& _data);

	// 线程主函数
	static void execute(DataType _data);

	// 交换数据
	static auto exchange(AtomicType& _atomic, const DataType& _data) noexcept
	{
		return _atomic.exchange(_data, std::memory_order::relaxed);
	}

private:
	// 加载非原子数据
	auto load() const noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

public:
	// 默认构造函数
	Thread() : _atomic(std::make_shared<Structure>()) { create(); }

	// 删除默认复制构造函数
	Thread(const Thread&) = delete;

	// 默认移动构造函数
	Thread(Thread&& _thread) noexcept
		: _atomic(exchange(_thread._atomic, nullptr)) {}

	// 默认析构函数
	~Thread() { destroy(); }

	// 删除默认复制赋值运算符函数
	Thread& operator=(const Thread&) = delete;

	// 默认移动赋值运算符函数
	Thread& operator=(Thread&& _thread) noexcept
	{
		exchange(_atomic, exchange(_thread._atomic, nullptr));
		return *this;
	}

	// 获取线程唯一标识
	ThreadID getID() const;

	// 是否闲置
	bool idle() const noexcept;

	// 创建线程
	bool create();

	// 销毁线程
	void destroy();

	// 配置任务队列与回调函数子
	bool configure(const Queue& _taskQueue, const Callback& _callback);

	// 配置单任务与回调函数子
	bool configure(const Functor& _task, const Callback& _callback);

	// 激活线程
	bool notify();
};

// 获取任务
template <typename _Functor, typename _Queue>
bool Thread<_Functor, _Queue>::setTask(DataType& _data)
{
	// 无任务队列
	if (not _data->_taskQueue)
		return false;

	auto result = _data->_taskQueue->pop();
	if (result)
	{
		_data->setState(State::RUNNABLE);
		_data->setTask(result.value());
	}
	return result.has_value();
}

// 线程主函数
template <typename _Functor, typename _Queue>
void Thread<_Functor, _Queue>::execute(DataType _data)
{
	// 条件变量的谓词，若任务有效，则无需等待通知
	auto predicate = [&_data] { return _data->getValidity(); };

	// 若谓词为真，自动解锁互斥元，阻塞线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	// 线程退出通道
	while (_data->getValidity() or _data->_condition)
	{
		_data->setState(State::RUNNING);

		// 执行函数子之时捕获异常，防止线程泄漏
		try
		{
			// 若任务函数子有效，执行任务
			if (_data->_task)
				_data->_task();
		}
		catch (std::exception& exception)
		{
			std::cerr << exception.what() << std::source_location::current() << std::endl;
		}

		// 执行完毕清除任务
		_data->_task = nullptr;

		// 配置新任务
		bool idle = not setTask(_data);
		auto callback = _data->_callback;
		if (idle)
			_data->setState(State::BLOCKED);

		// 若回调函数子有效，以闲置状态和线程标识为参数，执行回调函数子
		if (callback)
			callback(idle, _data->getID());

		// 根据谓词真假，决定是否阻塞线程
		_data->_condition.wait(predicate);
	}
}

// 获取线程ID
template <typename _Functor, typename _Queue>
//Thread<_Functor, _Queue>::ThreadID Thread<_Functor, _Queue>::getID() const
auto Thread<_Functor, _Queue>::getID() const -> ThreadID
{
	auto data = load();
	if (not data)
		return ThreadID();

	std::lock_guard lock(data->_threadMutex);
	return data->getID();
}

// 是否闲置
template <typename _Functor, typename _Queue>
bool Thread<_Functor, _Queue>::idle() const noexcept
{
	auto data = load();
	if (not data)
		return false;

	auto state = data->getState();
	return state == State::INITIAL or state == State::BLOCKED;
}

// 创建线程
template <typename _Functor, typename _Queue>
bool Thread<_Functor, _Queue>::create()
{
	auto data = load();
	if (not data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	if (data->getState() != State::EMPTY)
		return false;

	// 创建std::thread对象，以data为参数，执行函数execute
	data->_thread = std::thread(execute, data);
	data->setState(State::INITIAL);
	return true;
}

// 销毁线程
template <typename _Functor, typename _Queue>
void Thread<_Functor, _Queue>::destroy()
{
	auto data = load();
	if (not data)
		return;

	std::lock_guard lock(data->_threadMutex);
	if (data->getState() == State::EMPTY)
		return;

	// 通知线程退出
	data->_condition.exit();

	// 挂起直到线程退出
	if (data->_thread.joinable())
		data->_thread.join();

	// 清空配置项
	data->_callback = nullptr;
	data->_taskQueue = nullptr;
	data->setState(State::EMPTY);
}

// 配置任务队列与回调函数子
template <typename _Functor, typename _Queue>
bool Thread<_Functor, _Queue>::configure(const Queue& _taskQueue, const Callback& _callback)
{
	// 无任务队列
	if (not _taskQueue)
		return false;

	auto data = load();
	if (not data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	if (not idle())
		return false;

	data->_taskQueue = _taskQueue; // 配置任务队列，用于自动获取任务
	data->_callback = _callback; // 配置回调函数子，执行一次任务，通知守护线程，传递线程闲置状态
	data->setState(State::BLOCKED);
	return true;
}

// 配置单任务与回调函数子
template <typename _Functor, typename _Queue>
bool Thread<_Functor, _Queue>::configure(const Functor& _task, const Callback& _callback)
{
	// 任务无效
	if (not _task)
		return false;

	auto data = load();
	if (not data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	if (not idle())
		return false;

	data->setState(State::RUNNABLE);
	data->_callback = _callback; // 配置回调函数子
	data->setTask(_task); // 设置任务
	return true;
}

// 激活线程
template <typename _Functor, typename _Queue>
bool Thread<_Functor, _Queue>::notify()
{
	auto data = load();
	if (not data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	auto state = data->getState();

	// 若处于阻塞状态则获取任务
	if (state == State::BLOCKED and setTask(data))
		state = State::RUNNABLE;

	// 非就绪状态不必通知
	if (state != State::RUNNABLE)
		return false;

	data->_condition.notify_one(Condition::Strategy::RELAXED);
	return true;
}

ETERFREE_SPACE_END
