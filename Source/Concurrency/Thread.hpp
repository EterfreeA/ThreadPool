/*
* 文件名称：Thread.hpp
* 语言标准：C++20
* 
* 创建日期：2017年09月22日
* 更新日期：2023年02月07日
* 
* 摘要
* 1. 定义线程类模板Thread。
* 2. Thread提供线程重用方案，支持销毁再创建，一次创建反复使用。
* 3. 线程在创建之后进入阻塞状态，先调用函数configure分配任务，再调用函数notify激活线程。
* 4. 可选配置单任务或者获取函数子，以及回复函数子。
*   获取函数子用于自动获取任务；回复函数子用于回复线程池，告知已执行完单个任务，以及当前的闲置状态。
* 5. 在执行任务之时捕获异常，防止线程泄漏。
* 6. 线程在执行任务之后，倘若配置有获取函数子，则主动获取任务，否则进入阻塞状态。
*   倘若获取任务失败，等待分配任务；否则执行新任务，从而提高执行效率。
* 7. 线程在退出之前，倘若配置有获取函数子，则确保完成所有任务，否则仅执行配置的单个任务。
* 8. 以原子操作确保接口的线程安全性，以单状态枚举确保接口的执行顺序。
* 9. 引入强化条件类模板Condition，当激活先于阻塞之时，确保线程正常退出。
* 10.线程主函数声明为静态成员，除去与类成员指针this的关联性。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v3.0.1
* 变化
* v3.0.0
* 1.以获取函数子替换任务队列。
* 2.配置任务支持复制语义和移动语义。
* 3.解决线程在销毁又创建之时可能出现的状态错误问题。
* 4.判断获取的任务是否有效，以防止线程泄漏。
* v3.0.1
* 1.修复移动赋值运算符函数的资源泄漏问题。
*/

#pragma once

#include <functional>
#include <utility>
#include <memory>
#include <cstdint>
#include <exception>
#include <atomic>
#include <mutex>
#include <thread>

#include "Core/Common.hpp"
#include "Core/Logger.h"
#include "Condition.hpp"

ETERFREE_SPACE_BEGIN

template <typename _TaskType = std::function<void()>>
class Thread final
{
	// 状态枚举
	enum class State : std::uint8_t
	{
		EMPTY,		// 空态
		INITIAL,	// 初始态
		RUNNABLE,	// 就绪态
		RUNNING,	// 运行态
		BLOCKED,	// 阻塞态
	};

	// 线程数据结构体
	struct Structure;

private:
	using DataType = std::shared_ptr<Structure>;
	using Atomic = std::atomic<DataType>;
	using OrderType = std::memory_order;

public:
	using TaskType = _TaskType;
	using ThreadID = std::thread::id;

	using FetchType = std::function<bool(TaskType&)>;
	using ReplyType = std::function<void(ThreadID, bool)>;

	using Condition = Condition<>;
	using SizeType = Condition::Size;

private:
	Atomic _atomic;

private:
	// 交换数据
	static auto exchange(Atomic& _atomic, \
		const DataType& _data) noexcept
	{
		return _atomic.exchange(_data, \
			OrderType::relaxed);
	}

	// 销毁线程
	static void destroy(DataType&& _data);

	// 获取任务
	static bool getTask(DataType& _data);

	// 线程主函数
	static void execute(DataType _data);

private:
	// 加载非原子数据
	auto load() const noexcept
	{
		return _atomic.load(OrderType::relaxed);
	}

public:
	// 默认构造函数
	Thread() : \
		_atomic(std::make_shared<Structure>()) { create(); }

	// 删除默认复制构造函数
	Thread(const Thread&) = delete;

	// 默认移动构造函数
	Thread(Thread&& _another) noexcept : \
		_atomic(exchange(_another._atomic, nullptr)) {}

	// 默认析构函数
	~Thread() { destroy(); }

	// 删除默认复制赋值运算符函数
	Thread& operator=(const Thread&) = delete;

	// 默认移动赋值运算符函数
	Thread& operator=(Thread&& _another);

	// 获取线程唯一标识
	ThreadID getID() const;

	// 是否闲置
	bool idle() const noexcept;

	// 创建线程
	bool create();

	// 销毁线程
	void destroy()
	{
		destroy(load());
	}

	// 配置获取与回复函数子
	bool configure(const FetchType& _fetch, \
		const ReplyType& _reply);

	// 配置任务与回复函数子
	bool configure(const TaskType& _task, \
		const ReplyType& _reply);

	// 配置任务与回复函数子
	bool configure(TaskType&& _task, \
		const ReplyType& _reply);

	// 激活线程
	bool notify();
};

// 线程数据结构体
template <typename _TaskType>
struct Thread<_TaskType>::Structure
{
	std::mutex _threadMutex;		// 线程互斥元
	std::thread _thread;			// 线程实体

	Condition _condition;			// 强化条件变量
	std::atomic<State> _state;		// 原子状态

	mutable std::mutex _taskMutex;	// 任务互斥元
	TaskType _task;					// 任务函数子

	FetchType _fetch;				// 获取函数子
	ReplyType _reply;				// 回复函数子

	Structure() : _state(State::EMPTY) {}

	// 获取线程唯一标识
	auto getID() const noexcept
	{
		return _thread.get_id();
	}

	// 获取状态
	auto getState() const noexcept
	{
		return _state.load(OrderType::relaxed);
	}

	// 设置状态
	void setState(State _state) noexcept
	{
		this->_state.store(_state, \
			OrderType::relaxed);
	}

	// 任务有效性
	bool getValidity() const
	{
		std::lock_guard lock(_taskMutex);
		return static_cast<bool>(_task);
	}

	// 获取任务
	bool getTask(TaskType& _task);

	// 设置任务
	void setTask(const TaskType& _task)
	{
		std::lock_guard lock(_taskMutex);
		this->_task = _task;
	}
	void setTask(TaskType&& _task)
	{
		std::lock_guard lock(_taskMutex);
		this->_task = std::forward<TaskType>(_task);
	}
};

// 获取任务
template <typename _TaskType>
bool Thread<_TaskType>::Structure::getTask(TaskType& _task)
{
	std::lock_guard lock(_taskMutex);
	_task = std::move(this->_task);
	return static_cast<bool>(_task);
}

// 销毁线程池
template <typename _TaskType>
void Thread<_TaskType>::destroy(DataType&& _data)
{
	if (not _data) return;

	std::lock_guard lock(_data->_threadMutex);
	if (_data->getState() == State::EMPTY)
		return;

	// 通知线程退出
	_data->_condition.exit();

	// 挂起直到线程退出
	if (_data->_thread.joinable())
		_data->_thread.join();

	// 清空配置项
	_data->_fetch = nullptr;
	_data->_reply = nullptr;
	_data->setState(State::EMPTY);
}

// 获取任务
template <typename _TaskType>
bool Thread<_TaskType>::getTask(DataType& _data)
{
	if (not _data->_fetch) return false;

	decltype(_data->_task) task;
	if (not _data->_fetch(task) or not task)
		return false;

	_data->setState(State::RUNNABLE);
	_data->setTask(std::move(task));
	return true;
}

// 线程主函数
template <typename _TaskType>
void Thread<_TaskType>::execute(DataType _data)
{
	// 条件变量的谓词，若任务有效，则无需等待通知
	auto predicate = [&_data]
	{ return _data->getValidity(); };

	// 若谓词为真，自动解锁互斥元，阻塞线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	// 线程退出通道
	while (_data->_condition \
		or _data->getValidity())
	{
		_data->setState(State::RUNNING);

		// 执行函数子之时捕获异常，防止线程泄漏
		try
		{
			// 函数子有效则执行任务
			if (decltype(_data->_task) task; \
				_data->getTask(task)) task();
		}
		catch (std::exception& exception)
		{
			Logger::output(Logger::Level::ERROR, \
				std::source_location::current(), exception);
		}

		auto reply = _data->_reply;

		// 获取新任务
		bool idle = not getTask(_data);
		if (idle)
			_data->setState(State::BLOCKED);

		// 若回复函数子有效，以线程标识和闲置状态为参数，执行回复函数子
		if (reply) reply(_data->getID(), idle);

		// 根据谓词真假，决定是否阻塞线程
		_data->_condition.wait(predicate);
	}
}

// 默认移动赋值运算符函数
template <typename _TaskType>
auto Thread<_TaskType>::operator=(Thread&& _another) \
-> Thread&
{
	if (&_another != this)
	{
		auto data = exchange(_another._atomic, nullptr);
		if (data = exchange(this->_atomic, data))
			destroy(std::move(data));
	}
	return *this;
}

// 获取线程ID
template <typename _TaskType>
auto Thread<_TaskType>::getID() const -> ThreadID
{
	auto data = load();
	if (not data) return ThreadID();

	std::lock_guard lock(data->_threadMutex);
	return data->getID();
}

// 是否闲置
template <typename _TaskType>
bool Thread<_TaskType>::idle() const noexcept
{
	auto data = load();
	if (not data) return false;

	auto state = data->getState();
	return state == State::INITIAL \
		or state == State::BLOCKED;
}

// 创建线程
template <typename _TaskType>
bool Thread<_TaskType>::create()
{
	auto data = load();
	if (not data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (data->getState() != State::EMPTY)
		return false;

	data->setState(State::INITIAL);

	data->_condition.enter();

	// 创建std::thread对象，以data为参数，执行函数execute
	data->_thread = std::thread(execute, data);
	return true;
}

// 配置获取与回复函数子
template <typename _TaskType>
bool Thread<_TaskType>::configure(const FetchType& _fetch, \
	const ReplyType& _reply)
{
	// 获取函数子无效
	if (not _fetch) return false;

	auto data = load();
	if (not data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (not idle()) return false;

	data->_fetch = _fetch; // 配置获取函数子，用于自动获取任务
	data->_reply = _reply; // 配置回复函数子，每执行一次任务，通知守护线程，传递线程闲置状态
	data->setState(State::BLOCKED);
	return true;
}

// 配置任务与回复函数子
template <typename _TaskType>
bool Thread<_TaskType>::configure(const TaskType& _task, \
	const ReplyType& _reply)
{
	// 任务无效
	if (not _task) return false;

	auto data = load();
	if (not data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (not idle()) return false;

	data->setState(State::RUNNABLE);
	data->_reply = _reply; // 配置回复函数子
	data->setTask(_task); // 设置任务函数子
	return true;
}

// 配置任务与回复函数子
template <typename _TaskType>
bool Thread<_TaskType>::configure(TaskType&& _task, \
	const ReplyType& _reply)
{
	// 任务无效
	if (not _task) return false;

	auto data = load();
	if (not data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (not idle()) return false;

	data->setState(State::RUNNABLE);
	data->_reply = _reply; // 配置回复函数子
	data->setTask(std::forward<TaskType>(_task)); // 设置任务函数子
	return true;
}

// 激活线程
template <typename _TaskType>
bool Thread<_TaskType>::notify()
{
	auto data = load();
	if (not data) return false;

	std::lock_guard lock(data->_threadMutex);
	auto state = data->getState();

	// 处于阻塞状态则获取任务
	if (state == State::BLOCKED \
		and getTask(data))
		state = State::RUNNABLE;

	// 非就绪状态不必通知
	if (state != State::RUNNABLE)
		return false;

	data->_condition.notify_one(Condition::Strategy::RELAXED);
	return true;
}

ETERFREE_SPACE_END
