#include "Thread.h"
#include "Queue.h"
#include "Condition.hpp"

#include <exception>
#include <iostream>
#include <atomic>
#include <mutex>

ETERFREE_BEGIN

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

	std::thread thread;			// 线程实体
	std::atomic<State> state;	// 原子状态
	std::mutex threadMutex;		// 线程互斥元
	Condition condition;		// 强化条件变量

	TaskQueue taskQueue;		// 任务队列
	std::mutex taskMutex;		// 任务互斥元
	Functor task;				// 任务函数子
	Callback callback;			// 回调函数子

	// 设置状态
	void setState(State state) noexcept
	{
		this->state.store(state, std::memory_order::memory_order_relaxed);
	}

	// 获取状态
	State getState() const noexcept
	{
		return state.load(std::memory_order::memory_order_relaxed);
	}

	// 设置任务
	void setTask(const Functor& task)
	{
		std::lock_guard lock(taskMutex);
		this->task = task;
	}

	// 任务是否有效
	bool getValidity()
	{
		std::lock_guard lock(taskMutex);
		return static_cast<bool>(task);
	}
};

// 获取任务
bool Thread::setTask(DataType& data)
{
	// 无任务队列
	if (data->taskQueue == nullptr)
		return false;

	// 无任务
	if (decltype(data->task) task; !data->taskQueue->pop(task))
		return false;
	// 有任务
	else
	{
		data->setState(Structure::State::RUNNABLE);
		data->setTask(task);
	}
	return true;

	//auto result = data->taskQueue->pop();
	//if (result)
	//{
	//	data->setState(Structure::State::RUNNABLE);
	//	data->setTask(result.value());
	//}
	//return static_cast<bool>(result);
}

// 线程主函数
void Thread::execute(DataType data)
{
	// 条件变量的谓词，若任务有效或者条件无效，则无需等待通知
	auto predicate = [&data] { return data->getValidity() || !data->condition.valid(); };

	// 若谓词为真，自动解锁互斥元，阻塞线程，直至通知激活，再次锁定互斥元
	data->condition.wait(predicate);

	// 线程退出通道
	while (data->getValidity() || data->condition.valid())
	{
		using State = Structure::State;
		data->setState(State::RUNNING);

		// 执行函数子之时捕获异常，防止线程泄漏
		try
		{
			// 若任务函数子有效，执行任务函数子
			if (data->task)
				data->task();
		}
		catch (std::exception& exception)
		{
			std::cerr << exception.what() << std::endl;
		}

		// 执行完毕清除任务
		data->task = nullptr;

		// 配置新任务
		bool idle = !setTask(data);
		if (idle)
			data->setState(State::BLOCKED);

		// 若回调函数子有效，以闲置状态和线程标识为参数，执行回调函数子
		if (data->callback)
			data->callback(idle, data->thread.get_id());

		// 根据谓词真假，决定是否阻塞线程
		data->condition.wait(predicate);
	}
}

// 默认构造函数
Thread::Thread()
	: data(std::make_shared<Structure>())
{
	data->setState(Structure::State::EMPTY);
	create();
}

// 默认析构函数
Thread::~Thread()
{
	// 支持移动语义
	if (data != nullptr)
		destroy();
}

// 获取线程ID
Thread::ThreadID Thread::getID()
{
	std::lock_guard lock(data->threadMutex);
	return data->thread.get_id();
}

// 是否空闲
bool Thread::free() const noexcept
{
	return idle();
}

// 是否闲置
bool Thread::idle() const noexcept
{
	using State = Structure::State;
	State state = data->getState();
	return state == State::INITIAL || state == State::BLOCKED;
}

// 创建线程
bool Thread::create()
{
	std::lock_guard lock(data->threadMutex);
	using State = Structure::State;
	if (data->getState() != State::EMPTY)
		return false;

	// 创建std::thread对象，以data为参数，执行函数execute
	data->thread = std::thread(Thread::execute, data);
	data->setState(State::INITIAL);
	return true;
}

// 销毁线程
void Thread::destroy()
{
	std::lock_guard lock(data->threadMutex);
	using State = Structure::State;
	if (data->getState() == State::EMPTY)
		return;

	// 通知线程退出
	data->condition.exit();

	// 挂起直到线程退出
	if (data->thread.joinable())
		data->thread.join();

	// 清空配置项
	data->callback = nullptr;
	data->taskQueue = nullptr;
	data->setState(State::EMPTY);
}

// 配置任务队列与回调函数子
bool Thread::configure(TaskQueue taskQueue, Callback callback)
{
	// 无任务队列
	if (taskQueue == nullptr)
		return false;

	std::lock_guard lock(data->threadMutex);
	if (!idle())
		return false;

	data->taskQueue = taskQueue; // 配置任务队列，用以自动获取任务
	data->callback = callback; // 配置回调函数子，每执行一次任务，通知守护线程，传递线程闲置状态
	data->setState(Structure::State::BLOCKED);
	return true;
}

// 配置单任务与回调函数子
bool Thread::configure(const Functor& task, Callback callback)
{
	// 任务无效
	if (!task)
		return false;

	std::lock_guard lock(data->threadMutex);
	if (!idle())
		return false;

	data->setState(Structure::State::RUNNABLE);
	data->callback = callback; // 配置回调函数子
	data->setTask(task); // 设置任务
	return true;
}

// 启动线程
bool Thread::start()
{
	return notify();
}

// 激活线程
bool Thread::notify()
{
	std::lock_guard lock(data->threadMutex);
	using State = Structure::State;
	State state = data->getState();

	// 若处于阻塞状态则获取任务
	if (state == State::BLOCKED && setTask(data))
		state = State::RUNNABLE;

	// 非就绪状态不必通知
	if (state != State::RUNNABLE)
		return false;

	data->condition.notify_one(Structure::Condition::Strategy::RELAXED);
	return true;
}

ETERFREE_END
