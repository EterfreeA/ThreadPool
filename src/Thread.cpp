#include "Thread.h"
#include "Condition.hpp"
#include "Queue.hpp"

#include <utility>
#include <exception>
#include <iostream>
#include <atomic>

ETERFREE_SPACE_BEGIN

// 线程数据结构体
struct Thread::Structure
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
		this->_state.store(_state, std::memory_order_relaxed);
	}

	// 获取状态
	auto getState() const noexcept
	{
		return _state.load(std::memory_order_relaxed);
	}

	// 设置任务
	void setTask(const Functor& _task)
	{
		std::lock_guard lock(_taskMutex);
		this->_task = _task;
	}
	void setTask(Functor&& _task)
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

// 获取任务
bool Thread::setTask(DataType& _data)
{
	// 无任务队列
	if (!_data->_taskQueue)
		return false;

	auto result = _data->_taskQueue->pop();
	if (result)
	{
		_data->setState(Structure::State::RUNNABLE);
		_data->setTask(result.value());
	}
	return result.has_value();
}

// 线程主函数
void Thread::execute(DataType _data)
{
	// 条件变量的谓词，若任务有效，则无需等待通知
	auto predicate = [&_data] { return _data->getValidity(); };

	// 若谓词为真，自动解锁互斥元，阻塞线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	// 线程退出通道
	while (_data->getValidity() || _data->_condition)
	{
		using State = Structure::State;
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
			std::cerr << exception.what() << std::endl;
		}

		// 执行完毕清除任务
		_data->_task = nullptr;

		// 配置新任务
		bool idle = !setTask(_data);
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

// 默认构造函数
Thread::Thread()
	: _data(std::make_shared<Structure>())
{
	create();
}

// 默认移动构造函数
Thread::Thread(Thread&& _thread)
{
	std::scoped_lock lock(_mutex, _thread._mutex);
	_data = std::move(_thread._data);
}

// 默认移动赋值运算符函数
Thread& Thread::operator=(Thread&& _thread)
{
	if (&_thread != this)
	{
		std::scoped_lock lock(_mutex, _thread._mutex);
		_data = std::move(_thread._data);
	}
	return *this;
}

// 获取线程ID
Thread::ThreadID Thread::getID() const
{
	auto data = load();
	if (!data)
		return ThreadID();

	std::lock_guard lock(data->_threadMutex);
	return data->getID();
}

// 是否闲置
bool Thread::idle() const
{
	auto data = load();
	if (!data)
		return false;

	using State = Structure::State;
	auto state = data->getState();
	return state == State::INITIAL || state == State::BLOCKED;
}

// 创建线程
bool Thread::create()
{
	auto data = load();
	if (!data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	using State = Structure::State;
	if (data->getState() != State::EMPTY)
		return false;

	// 创建std::thread对象，以data为参数，执行函数execute
	data->_thread = std::thread(Thread::execute, data);
	data->setState(State::INITIAL);
	return true;
}

// 销毁线程
void Thread::destroy()
{
	auto data = load();
	if (!data)
		return;

	std::lock_guard lock(data->_threadMutex);
	using State = Structure::State;
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
bool Thread::configure(const Queue& _taskQueue, const Callback& _callback)
{
	// 无任务队列
	if (!_taskQueue)
		return false;

	auto data = load();
	if (!data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	if (!idle())
		return false;

	data->_taskQueue = _taskQueue; // 配置任务队列，用于自动获取任务
	data->_callback = _callback; // 配置回调函数子，执行一次任务，通知守护线程，传递线程闲置状态
	data->setState(Structure::State::BLOCKED);
	return true;
}

// 配置单任务与回调函数子
bool Thread::configure(const Functor& _task, const Callback& _callback)
{
	// 任务无效
	if (!_task)
		return false;

	auto data = load();
	if (!data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	if (!idle())
		return false;

	data->setState(Structure::State::RUNNABLE);
	data->_callback = _callback; // 配置回调函数子
	data->setTask(_task); // 设置任务
	return true;
}

// 激活线程
bool Thread::notify()
{
	auto data = load();
	if (!data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	using State = Structure::State;
	auto state = data->getState();

	// 若处于阻塞状态则获取任务
	if (state == State::BLOCKED && setTask(data))
		state = State::RUNNABLE;

	// 非就绪状态不必通知
	if (state != State::RUNNABLE)
		return false;

	data->_condition.notify_one(Structure::Condition::Strategy::RELAXED);
	return true;
}

ETERFREE_SPACE_END
