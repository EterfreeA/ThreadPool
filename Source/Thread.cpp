#include "Thread.h"
#include "Condition.hpp"
#include "DoubleQueue.hpp"

#include <utility>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <atomic>

ETERFREE_SPACE_BEGIN

// 线程数据结构体
struct Thread::Structure
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

	std::mutex _threadMutex;		// 线程互斥元
	std::thread _thread;			// 线程实体

	Condition<> _condition;			// 强化条件变量
	std::atomic<State> _state;		// 原子状态

	mutable std::mutex _taskMutex;	// 任务互斥元
	TaskType _task;					// 任务函数子

	QueueType _taskQueue;			// 任务队列
	Callback _callback;				// 回调函数子

	Structure() : _state(State::EMPTY) {}

	// 获取线程唯一标识
	auto getID() const noexcept
	{
		return _thread.get_id();
	}

	// 获取状态
	auto getState() const noexcept
	{
		return _state.load(std::memory_order_relaxed);
	}

	// 设置状态
	void setState(State _state) noexcept
	{
		this->_state.store(_state, \
			std::memory_order_relaxed);
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
bool Thread::Structure::getTask(TaskType& _task)
{
	std::lock_guard lock(_taskMutex);
	_task = std::move(this->_task);
	return static_cast<bool>(_task);
}

// 移动数据
auto Thread::move(Thread& _left, Thread&& _right) \
-> DataType
{
	std::lock_guard leftLock(_left._mutex);
	auto data = std::move(_left._data);

	std::lock_guard rightLock(_right._mutex);
	_left._data = std::move(_right._data);
	return data;
}

// 销毁线程
void Thread::destroy(DataType&& _data)
{
	using State = Structure::State;

	if (!_data) return;

	std::lock_guard lock(_data->_threadMutex);
	if (_data->getState() == State::EMPTY)
		return;

	// 通知线程退出
	_data->_condition.exit();

	// 挂起直到线程退出
	if (_data->_thread.joinable())
		_data->_thread.join();

	// 清空配置项
	_data->_taskQueue.reset();
	_data->_callback = nullptr;
	_data->setState(State::EMPTY);
}

// 获取任务
bool Thread::getTask(DataType& _data)
{
	using State = Structure::State;

	if (!_data->_taskQueue)
		return false;

	decltype(_data->_task) task;
	if (!_data->_taskQueue->pop(task))
		return false;

	if (!task) task = [] {};

	_data->setState(State::RUNNABLE);
	_data->setTask(std::move(task));
	return true;
}

// 线程主函数
void Thread::execute(DataType _data)
{
	using State = Structure::State;

	// 条件变量的谓词，若任务有效，则无需等待通知
	auto predicate = [&_data]
	{ return _data->getValidity(); };

	// 若谓词为真，自动解锁互斥元，阻塞线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	// 线程退出通道
	while (_data->_condition \
		|| _data->getValidity())
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
			std::ostringstream stream;
			stream << exception.what() << std::endl;
			std::clog << stream.str();
		}

		auto callback = _data->_callback;

		// 获取新任务
		bool idle = !getTask(_data);
		if (idle)
			_data->setState(State::BLOCKED);

		// 若回调函数子有效，以线程标识和闲置状态为参数，执行回调函数子
		if (callback)
			callback(_data->getID(), idle);

		// 根据谓词真假，决定是否阻塞线程
		_data->_condition.wait(predicate);
	}
}

// 默认构造函数
Thread::Thread() : \
	_data(std::make_shared<Structure>())
{
	create();
}

// 默认移动构造函数
Thread::Thread(Thread&& _another) noexcept
{
	try
	{
		std::lock_guard lock(_another._mutex);
		this->_data = std::move(_another._data);
	}
	catch (std::exception&) {}
}

// 默认析构函数
Thread::~Thread() noexcept
{
	try
	{
		destroy();
	}
	catch (std::exception&) {}
}

// 默认移动赋值运算符函数
auto Thread::operator=(Thread&& _thread) noexcept \
-> Thread&
{
	if (&_thread != this)
	{
		try
		{
			auto data = move(*this, \
				std::forward<Thread>(_thread));

			destroy(std::move(data));
		}
		catch (std::exception&) {}
	}
	return *this;
}

// 获取线程ID
auto Thread::getID() const -> ThreadID
{
	auto data = load();
	if (!data) return ThreadID();

	std::lock_guard lock(data->_threadMutex);
	return data->getID();
}

// 是否闲置
bool Thread::idle() const
{
	using State = Structure::State;

	auto data = load();
	if (!data) return false;

	auto state = data->getState();
	return state == State::INITIAL \
		|| state == State::BLOCKED;
}

// 创建线程
bool Thread::create()
{
	using State = Structure::State;

	auto data = load();
	if (!data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (data->getState() != State::EMPTY)
		return false;

	data->setState(State::INITIAL);

	data->_condition.enter();

	// 创建std::thread对象，以data为参数，执行函数execute
	data->_thread = std::thread(execute, data);
	return true;
}

// 配置任务队列与回调函数子
bool Thread::configure(const QueueType& _taskQueue, \
	const Callback& _callback)
{
	// 任务队列无效
	if (!_taskQueue) return false;

	auto data = load();
	if (!data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (!idle()) return false;

	data->_taskQueue = _taskQueue; // 配置任务队列，用于自动获取任务
	data->_callback = _callback; // 配置回调函数子，每执行一次任务，通知守护线程，传递线程闲置状态
	data->setState(Structure::State::BLOCKED);
	return true;
}

// 配置单任务与回调函数子
bool Thread::configure(const TaskType& _task, \
	const Callback& _callback)
{
	// 任务无效
	if (!_task) return false;

	auto data = load();
	if (!data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (!idle()) return false;

	data->setState(Structure::State::RUNNABLE);
	data->_callback = _callback; // 配置回调函数子
	data->setTask(_task); // 设置任务函数子
	return true;
}

// 配置单任务与回调函数子
bool Thread::configure(TaskType&& _task, \
	const Callback& _callback)
{
	// 任务无效
	if (!_task) return false;

	auto data = load();
	if (!data) return false;

	std::lock_guard lock(data->_threadMutex);
	if (!idle()) return false;

	data->setState(Structure::State::RUNNABLE);
	data->_callback = _callback; // 配置回调函数子
	data->setTask(std::forward<TaskType>(_task)); // 设置任务函数子
	return true;
}

// 激活线程
bool Thread::notify()
{
	using Policy = Condition<>::Policy;
	using State = Structure::State;

	auto data = load();
	if (!data)
		return false;

	std::lock_guard lock(data->_threadMutex);
	auto state = data->getState();

	// 处于阻塞状态则获取任务
	if (state == State::BLOCKED \
		&& getTask(data))
		state = State::RUNNABLE;

	// 非就绪状态不必通知
	if (state != State::RUNNABLE)
		return false;

	data->_condition.notify_one(Policy::RELAXED);
	return true;
}

ETERFREE_SPACE_END
