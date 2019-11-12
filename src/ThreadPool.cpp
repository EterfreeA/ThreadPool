#include "ThreadPool.h"
#include "Thread.h"
#include "Queue.h"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <algorithm>

ETERFREE_BEGIN

// 线程池数据结构体
struct ThreadPool::Structure
{
	using TaskQueue = Queue<ThreadPool::functor>;
	std::vector<std::unique_ptr<Thread>> threadTable;		// 线程表
	std::shared_ptr<TaskQueue> taskQueue;					// 任务队列
	std::function<void(bool, Thread::ThreadID)> callback;	// 回调函数子
	std::thread thread;										// 守护线程
	std::mutex mutex;										// 互斥元
	std::condition_variable condition;						// 条件变量
	std::atomic_bool closed;								// 关闭标记
	//std::atomic_int timeSlice;
	std::atomic<size_type> maxThreads;						// 最大线程数量
	std::atomic<size_type> freeThreads;						// 空闲线程数量
	// 构造函数
	Structure()
		: taskQueue(std::make_shared<TaskQueue>()) {}
};

// 默认构造函数
ThreadPool::ThreadPool(size_type threads, size_type maxThreads)
	/* 智能指针std::shared_ptr需要维护引用计数，若调用构造函数（即先以new运算符创建对象，再传递给std::shared_ptr），
	一共申请两次内存，先申请对象内存，再申请控制块内存，对象内存和控制块内存不连续。
	而使用std::make_shared方法只申请一次内存，对象内存和控制块内存连续。 */
	: data(std::make_shared<Structure>())
{
	setClosed(data, false);	// 线程池设为未关闭状态
	//setTimeSlice(timeSlice);
	setMaxThreads(maxThreads);	// 设置最大线程数量
	// 保证线程数量不超过最大线程数量
	threads = std::min(threads, maxThreads);

	/* 定义Lambda函数，线程主动于任务队列获取任务，获取失败时回调通知线程池，
	空闲线程数量加一，若未增加之前，空闲线程数量为零，则唤醒阻塞的守护线程。 */
	data->callback = [data = std::weak_ptr(data)](bool free, Thread::ThreadID)
	{
		if (free)
		{
			auto shared_data = data.lock();
			if (shared_data && ++shared_data->freeThreads == 0x01)
				shared_data->condition.notify_one();
		}
	};

	data->threadTable.reserve(threads);	// 预分配内存空间，但是不初始化内存，即未调用构造函数
	// 初始化线程并放入线程表
	for (decltype(threads) counter = 0; counter < threads; ++counter)
	{
		auto thread = std::make_unique<Thread>();
		thread->configure(data->taskQueue, data->callback);
		data->threadTable.push_back(std::move(thread));
	}
	data->freeThreads = data->threadTable.size();	// 设置空闲线程数量
	// 创建std::thread对象，即守护线程，以data为参数，执行execute函数
	data->thread = std::thread(ThreadPool::execute, data);
}

// 默认析构函数
ThreadPool::~ThreadPool()
{
	destroy();
}

// 获取支持的并发线程数量
ThreadPool::size_type ThreadPool::getConcurrency()
{
	return std::thread::hardware_concurrency();
}

//// 设置管理器轮询时间片
//bool ThreadPool::setTimeSlice(size_type timeSlice)
//{
//	if (timeSlice < 0)
//		return false;
//	data->timeSlice = timeSlice;
//	return true;
//}
//
//// 获取管理器轮询时间片
//ThreadPool::size_type ThreadPool::getTimeSlice() const
//{
//	return data->timeSlice;
//}

// 设置最大线程数量
void ThreadPool::setMaxThreads(size_type maxThreads)
{
	data->maxThreads = maxThreads > 0 ? maxThreads : 0x01;
}

// 获取最大线程数量
ThreadPool::size_type ThreadPool::getMaxThreads() const
{
	return data->maxThreads;
}

// 设置线程数量
bool ThreadPool::setThreads(size_type threads)
{
	// 保证线程数量不超过上限
	if (threads > getMaxThreads())
		return false;
	// 增加线程
	if (auto number = threads - data->threadTable.size();
		number > 0)
	{
		std::unique_lock locker(data->mutex);	// 死锁隐患
		data->threadTable.reserve(threads);	// 增加线程表容量
		// 向线程表添加线程
		for (decltype(number) counter = 0; counter < number; ++counter)
		{
			auto thread = std::make_unique<Thread>();
			thread->configure(data->taskQueue, data->callback);
			data->threadTable.push_back(std::move(thread));
		}
		locker.unlock();

		data->freeThreads += number;
		// 如果添加线程之前无空闲线程，唤醒或许阻塞的守护线程
		if (data->freeThreads == number)
			data->condition.notify_one();
		return true;
	}
	// 减少线程（未制定策略）
	else if (number < 0)
	{
		return false;
	}
	return false;
}

// 获取线程数量
ThreadPool::size_type ThreadPool::getThreads() const
{
	return data->threadTable.size();
}

// 获取空闲线程数量
ThreadPool::size_type ThreadPool::getFreeThreads() const
{
	return data->freeThreads;
}

// 获取任务数量
ThreadPool::size_type ThreadPool::getTasks() const
{
	return data->taskQueue->size();
}

// 向任务队列添加单任务
void ThreadPool::pushTask(functor&& task)
{
	// 过滤空任务，防止守护线程配置任务时无法启动线程
	if (task == nullptr)
		return;
	data->taskQueue->push(std::move(task));
	// 如果添加任务之前任务队列为空，唤醒或许阻塞的守护线程
	if (data->taskQueue->size() == 0x01)
		data->condition.notify_one();
}

// 向任务队列批量添加任务
void ThreadPool::pushTask(std::list<functor>& tasks)
{
	// 过滤空任务，防止守护线程配置任务时无法启动线程
	for (auto it = tasks.cbegin(); it != tasks.cend(); ++it)
		if (*it == nullptr)
			it = tasks.erase(it);
		else
			++it;
	if (auto size = tasks.size(); size > 0)
	{
		data->taskQueue->push(tasks);
		// 如果添加任务之前任务队列为空，唤醒或许阻塞的守护线程
		if (data->taskQueue->size() == size)
			data->condition.notify_one();
	}
}

// 设置关闭状态
inline void ThreadPool::setClosed(data_type& data, bool closed)
{
	data->closed = closed;
}

// 获取关闭状态
inline bool ThreadPool::getClosed(const data_type& data)
{
	return data->closed;
}

// 守护线程主函数
void ThreadPool::execute(data_type data)
{
	/* 创建std::unique_lock对象，作为线程互斥锁，指定延迟锁定策略，用于互斥访问线程表。
	由于析构互斥锁之时，自动释放互斥元，因此不必手动释放。 */
	using std::defer_lock;
	std::unique_lock threadLocker(data->mutex, defer_lock);
	// 创建任务互斥锁，延迟锁定任务队列互斥元，用于互斥访问任务队列
	std::unique_lock taskLocker(data->taskQueue->mutex(), defer_lock);

	while (!getClosed(data))	// 守护线程退出通道
	{
		threadLocker.lock();	// 锁定线程互斥元
		/* 检查空闲线程数量和线程池关闭状态。
		如果无空闲线程并且线程池未关闭，守护进程进入阻塞状态，等待条件变量的唤醒信号；否则守护线程继续顺序执行指令。
		当条件变量唤醒守护进程时，再次检查空闲线程数量和线程池关闭状态。 */
		data->condition.wait(threadLocker,
			[&data] { return data->freeThreads || getClosed(data); });
		// 若线程池设为关闭状态，退出循环，结束守护线程
		if (getClosed(data))
			break;

		// 遍历线程表，给空闲线程分配任务
		for (auto it = data->threadTable.begin();
			it != data->threadTable.end() && data->freeThreads && !getClosed(data); ++it)
		{
			if (auto& thread = *it; thread->free())	// 若线程处于空闲状态
			{
				taskLocker.lock();	// 锁定任务队列互斥元
				data->condition.wait(taskLocker,
					[&data] { return !data->taskQueue->empty() || getClosed(data); });
				if (getClosed(data))
					return;

				if (thread->configure(data->taskQueue->front())		// 为线程分配新任务
					&& thread->start())	// 唤醒阻塞的线程
				{
					data->taskQueue->pop();	// 任务队列弹出已经配置的任务
					--data->freeThreads;	// 空闲线程数量减一
				}
				taskLocker.unlock();	// 释放任务队列互斥元
			}
		}
		threadLocker.unlock();	// 释放线程互斥元
	}
}

//bool ThreadPool::getTask(std::shared_ptr<Thread> thread)
//{
//	std::unique_lock<std::mutex> locker(data->tasks->mutex());
//	if (!data->tasks->empty())	// 任务队列非空
//	{
//		thread->configure(std::move(data->tasks->front()));	// 为线程配置新任务
//		data->tasks->pop();	// 任务队列弹出已经配置的任务
//		return true;
//	}
//	locker.unlock();
//
//	// 任务队列为空，空闲线程数量加一，若增加之前空闲线程数量为零，则唤醒阻塞的守护线程
//	if (++data->freeThreads == 0x01)
//		data->condition.notify_one();
//	return false;
//}

// 销毁线程池
void ThreadPool::destroy()
{
	// 若数据为空或者线程池已关闭，无需销毁线程池，以支持移动语义
	if (data == nullptr || getClosed(data))
		return;
	setClosed(data, true);	// 线程池设为关闭状态，即销毁状态

	data->thread.detach();	// 分离线程池守护线程
	data->condition.notify_one();	// 唤醒阻塞的守护线程
	//std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

ETERFREE_END
