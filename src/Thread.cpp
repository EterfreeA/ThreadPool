#include "Thread.h"
#include "Queue.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <exception>
#include <iostream>

ETERFREE_BEGIN

// 线程数据结构体
struct Thread::Structure
{
	std::thread thread;										// 工作线程
	std::mutex mutex;										// 互斥元
	std::condition_variable condition;						// 条件变量
	std::atomic_bool closed;								// 关闭状态标志
	std::atomic_bool running;								// 运行状态标志
	//ThreadPool *threadPool;								// 线程池指针
	std::shared_ptr<Queue<Thread::functor>> taskQueue;		// 任务队列
	std::function<void(bool, Thread::ThreadID)> callback;	// 回调函数子
	Thread::functor task;									// 任务函数子
	//// 线程过程参数解决方案：虚基类指针，过程类继承虚基类，通过强制类型转换，在过程函数中访问
	//void *vpParameters;
};

// 默认构造函数
Thread::Thread()
	: data(std::make_shared<Structure>())
{
	// 线程设为未关闭未运行状态
	setClosed(data, false);
	setRunning(data, false);
	// 创建std::thread对象，即工作线程，以data为参数，执行execute函数
	data->thread = std::thread(Thread::execute, data);
}

// 默认析构函数
Thread::~Thread()
{
	destroy();
}

// 任务队列及回调函数子配置方法
bool Thread::configure(std::shared_ptr<Queue<functor>> taskQueue,
	std::function<void(bool, ThreadID)> callback)
{
	if (getRunning(data))
		return false;
	//data->threadPool = threadPool;	// 指向线程池，便于自动从任务队列获取任务
	data->taskQueue = taskQueue;	// 指向任务队列，便于自动获取任务
	data->callback = callback;	// 回调函数子，用于通知守护线程获取任务失败，线程进入阻塞状态
	return true;
}

// 任务配置方法
bool Thread::configure(const functor& task)
{
	// 若处于运行状态，标志正在执行任务，配置新任务失败
	if (getRunning(data))
		return false;
	data->task = task;	// 配置任务函数子
	//data->vpParameters = vpParameters;
	return true;
}

// 启动工作线程
bool Thread::start()
{
	// 若任务为空，不必唤醒工作线程，防止虚假唤醒
	if (data->task == nullptr)
		return false;
	data->condition.notify_one();	// 通过条件变量唤醒工作线程
	return true;
}

// 获取线程ID
Thread::ThreadID Thread::getThreadID() const
{
	return data->thread.get_id();
}

// 获取空闲状态
bool Thread::free() const
{
	return !getRunning(data);
}

//const void *Thread::getParameters()
//{
//	return data->vpParameters;
//}

// 设置关闭状态
inline void Thread::setClosed(data_type& data, bool closed)
{
	data->closed = closed;
}

// 获取关闭状态
inline bool Thread::getClosed(const data_type& data)
{
	return data->closed;
}

// 设置运行状态
inline void Thread::setRunning(data_type& data, bool running)
{
	data->running = running;
}

// 获取运行状态
inline bool Thread::getRunning(const data_type& data)
{
	return data->running;
}

// 工作线程主函数
void Thread::execute(data_type data)
{
	//// 调用std::mem_fn获取函数子getTask，其拥有指向ThreadPool::getTask的指针，并且重载运算符operator()
	//auto &&getTask = std::mem_fn(&ThreadPool::getTask);

	// 创建std::unique_lock对象，作为线程互斥锁，未指定锁策略，默认立即锁住互斥元，用于阻塞工作线程
	std::unique_lock threadLocker(data->mutex);
	/* 条件变量通过线程互斥锁判断是否锁定互斥元。
	若已锁定互斥元，自动释放互斥元，工作线程进入阻塞状态，等待条件变量的唤醒信号。
	当条件变量唤醒唤醒工作线程时，再次锁定互斥元。 */
	data->condition.wait(threadLocker);

	// 若任务队列指针非空，创建任务互斥锁，指定延迟锁定策略，用于从任务队列互斥读取任务
	std::unique_lock<std::mutex> taskLocker;
	if (data->taskQueue)
		taskLocker = decltype(taskLocker)(data->taskQueue->mutex(), std::defer_lock);

	while (!getClosed(data) || data->task)	// 工作线程退出通道
	{
		setRunning(data, true);	// 线程设为运行状态
		// 执行函数子时捕获异常，防止线程泄漏
		try
		{
			// 若任务函数子非空，执行任务函数子
			if (data->task)
				data->task();
			//else
			//	process();	// 执行默认任务函数
		}
		catch (std::exception& exception)
		{
			std::cerr << exception.what() << std::endl;
		}
		data->task = nullptr;	// 执行完毕清除任务

		/* 以data->threadPool->getTask(this->shared_from_this())的形式，
		调用ThreadPool::getTask函数获取线程池任务队列的任务。
		若未成功获取任务，阻塞工作线程。 */
		//if (!(data->threadPool
		//	&& getTask(data->threadPool, this->shared_from_this())))
		//{
		//	// 工作线程退出通道
		//	if (getClosed())
		//		break;
		//	setRunning(false);	// 允许守护线程分配任务
		//	data->condition.wait(threadLocker);	// 工作线程进入阻塞状态，等待条件变量的唤醒信号
		//}

		// 工作线程退出通道
		if (getClosed(data))
			break;

		// 若任务队列指针非空，并且队列非空，则配置新任务
		if (data->taskQueue)
		{
			taskLocker.lock();
			if (!data->taskQueue->empty())
			{
				data->task = data->taskQueue->front();
				data->taskQueue->pop();
			}
			taskLocker.unlock();
		}

		auto empty = data->task == nullptr;	// 空任务标志，用于判断是否进入阻塞状态
		// 若回调函数子非空，执行回调函数子
		if (data->callback)
			data->callback(empty, data->thread.get_id());
		// 根据空任务标志设置线程运行状态，若任务为空，线程设为未运行状态，否则线程设为运行状态
		setRunning(data, !empty);
		/* 在工作线程阻塞之前，判断任务是否为空，以及线程是否设为关闭状态。
		若任务非空或者线程已关闭，放弃阻塞工作线程，以防止唤醒先于阻塞。*/
		if (data->task == nullptr && !getClosed(data))
			data->condition.wait(threadLocker);
		//// 进入阻塞状态，等待条件变量的唤醒消息，直到配置新任务或者关闭线程
		//data->condition.wait(threadLocker,
		//	[&data] { return data->task || getClosed(data); });
	}
}

//// 线程默认任务函数，用于继承扩展线程，形成钩子
//void Thread::process()
//{
//}

// 销毁线程
void Thread::destroy()
{
	// 若数据为空或者线程已关闭，无需销毁线程，以支持移动语义
	if (data == nullptr || getClosed(data))
		return;
	setClosed(data, true);	// 线程设为关闭状态，即销毁状态

	// 工作线程或许处于阻塞状态，通过条件变量唤醒工作线程
	data->condition.notify_one();
	// 阻塞线程直到工作线程结束
	if (data->thread.joinable())
		data->thread.join();
	// 预留等待时间
	//std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

ETERFREE_END
