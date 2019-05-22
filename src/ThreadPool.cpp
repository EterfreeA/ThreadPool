#include "Thread.h"
#include "ThreadPool.h"
#include "Queue.h"

#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>

// 线程池数据结构体
struct ThreadPoolStructure
{
	std::vector<std::shared_ptr<Thread>> vThreads;		// 工作线程表
	Queue<TaskPair> tasks;				// 任务队列
	std::thread thread;					// 线程
	std::mutex threadMutex;				// 线程互斥元
	std::condition_variable signal;		// 条件变量
	std::atomic_bool bClosed;			// 关闭标记
	std::atomic_uint maxThreads;		// 最大线程数
	std::atomic_uint freeThreads;		// 空闲线程数
	//std::atomic_int timeSlice;
};

// 线程池构造函数
ThreadPool::ThreadPool(unsigned nThread, unsigned maxThreads)
{
	threadPoolData = std::make_unique<ThreadPoolStructure>();
	setCloseStatus(false);	// 设置线程未关闭
	//setTimeSlice(timeSlice);
	setMaxThreads(maxThreads);	// 设置最大线程数量
	if (nThread > getMaxThreads())	// 保证工作线程不超过最大线程数
		nThread = getMaxThreads();
	threadPoolData->vThreads.reserve(nThread);	// 预分配内存空间，但是不初始化内存，即未调用构造函数
	/* shared_ptr需要维护引用计数，若调用构造函数（即通过new表达式创建对象，之后传递给shared_ptr），
	一共两次内存申请，先申请对象内存，再申请控制块内存，对象内存和控制块内存不连续。
	而使用make_shared方法只申请一次内存，对象内存和控制块内存在一起。 */
	for (unsigned i = 0; i < nThread; ++i)
		threadPoolData->vThreads.push_back(std::make_shared<Thread>(this));
	threadPoolData->freeThreads = threadPoolData->vThreads.size();	// 设置空闲线程数量
	threadPoolData->thread = std::thread(&ThreadPool::execute, this);	// 创建thread，在其中调用this对象的execute方法
}

// 线程池析构函数
ThreadPool::~ThreadPool()
{
	destroy();
}

// 设置线程池关闭状态，用于刚开始设置线程池状态和关闭线程池
inline void ThreadPool::setCloseStatus(bool bClosed)
{
	threadPoolData->bClosed = bClosed;
}

// 获取线程池的关闭状态
inline bool ThreadPool::getCloseStatus() const
{
	return threadPoolData->bClosed;
}

// 获取硬件设备并发运行的最大线程数量
unsigned ThreadPool::getMaxConcurrency()
{
	return std::thread::hardware_concurrency();
}

// 设置线程池工作线程数量
bool ThreadPool::setCurrentThreads(unsigned nThread)
{
	if (nThread > getMaxThreads())
		return false;
	auto result = nThread - threadPoolData->vThreads.size();
	if (result > 0)	// 增加工作线程
	{
		std::lock_guard<std::mutex> threadLocker(threadPoolData->threadMutex);	// 死锁
		threadPoolData->vThreads.reserve(nThread);	// 增大工作线程表容量
													// 向工作线程表中添加工作线程
		for (unsigned i = 0; i < result; ++i)
			threadPoolData->vThreads.push_back(std::make_shared<Thread>(this));
		threadPoolData->freeThreads += result;
		// 如果未添加工作线程时，空闲线程表为空，唤醒阻塞的管理线程
		if (threadPoolData->freeThreads == result)
			threadPoolData->signal.notify_one();
		return true;
	}
	else if (result < 0)	// 减少工作线程（未制定策略）
	{
		return false;
	}
	return false;
}

// 获取线程池中当前工作线程数量
unsigned ThreadPool::getCurrentThreads() const
{
	//std::lock_guard<std::mutex> threadLocker(threadPoolData->threadMutex);	// 死锁
	return threadPoolData->vThreads.size();
}

// 获取待完成任务队列中的任务数
unsigned ThreadPool::getTasks() const
{
	return threadPoolData->tasks.size();
}

// 设置线程池工作线程的最大数量
void ThreadPool::setMaxThreads(unsigned maxThreads)
{
	threadPoolData->maxThreads = maxThreads ? maxThreads : 1;
}

// 获取线程池最大工作线程数量
unsigned ThreadPool::getMaxThreads() const
{
	return threadPoolData->maxThreads;
}

//// 设置线程池管理器轮询时间片
//bool ThreadPool::setTimeSlice(unsigned timeSlice)
//{
//	if (timeSlice < 0)
//		return false;
//	threadPoolData->timeSlice = timeSlice;
//	return true;
//}
//
//// 获取线程池管理器轮询时间片
//unsigned ThreadPool::getTimeSlice() const
//{
//	return threadPoolData->timeSlice;
//}

// 向任务队列中添加单任务
void ThreadPool::pushTask(std::function<void()> run, std::function<void()> callback)
{
	threadPoolData->tasks.push(TaskPair(std::move(run), std::move(callback)));
	// 未添加任务之前，任务队列为空时，唤醒阻塞的管理线程
	if (threadPoolData->tasks.size() == 1)
		threadPoolData->signal.notify_one();
}

// 向任务队列中添加单任务
void ThreadPool::pushTask(TaskPair &&task)
{
	threadPoolData->tasks.push(std::move(task));
	// 未添加任务之前，任务队列为空时，唤醒阻塞的管理线程
	if (threadPoolData->tasks.size() == 1)
		threadPoolData->signal.notify_one();
}

// 向任务队列中批量添加任务
void ThreadPool::pushTask(std::list<TaskPair> &tasks)
{
	auto size = tasks.size();
	threadPoolData->tasks.push(tasks);
	if (threadPoolData->tasks.size() == size)
		threadPoolData->signal.notify_one();
}

// 销毁线程池
void ThreadPool::destroy()
{
	if (getCloseStatus())	// 若已经销毁线程池，忽略以下步骤
		return;
	threadPoolData->thread.detach();	// 分离线程池管理线程
	setCloseStatus(true);	// 设置线程池为关闭状态，即销毁状态
	threadPoolData->signal.notify_one();	// 唤醒阻塞的管理线程
	std::unique_lock<std::mutex> threadLocker(threadPoolData->threadMutex);
	//std::this_thread::sleep_for(std::chrono::milliseconds(10));
	// 遍历工作线程表，销毁所有工作线程
	/*for each (auto &thread in threadPoolData->vThreads)
	thread->destroy();*/
	/*for (auto it = threadPoolData->vThreads.cbegin(); it != threadPoolData->vThreads.cend(); ++it)
	it->get()->destroy();*/
	for (auto &thread : threadPoolData->vThreads)
		thread->destroy();
	threadLocker.unlock();
}

bool ThreadPool::getTask(std::shared_ptr<Thread> thread)
{
	std::unique_lock<std::mutex> taskLocker(threadPoolData->tasks.mutex());
	if (!threadPoolData->tasks.empty())	// 任务队列非空
	{
		thread->configure(std::move(threadPoolData->tasks.front()));	// 为工作线程配置新任务
		threadPoolData->tasks.pop();	// 弹出已经配置过的任务
		return true;
	}
	taskLocker.unlock();
	// 任务队列为空，空闲线程数量加一，若未增加空闲线程之前，空闲线程数量为零时，唤醒阻塞的管理线程
	if (++threadPoolData->freeThreads == 1)
		threadPoolData->signal.notify_one();
	return false;
}

// 线程池管理线程体
void ThreadPool::execute()
{
	std::unique_lock<std::mutex> threadLocker(threadPoolData->threadMutex, std::defer_lock);
	std::unique_lock<std::mutex> taskLocker(threadPoolData->tasks.mutex(), std::defer_lock);
	while (!getCloseStatus())	// 线程池管理线程体退出通道
	{
		threadLocker.lock();
		if (!threadPoolData->freeThreads)	// 若无空闲线程
		{
			// 阻塞此线程，再次获取线程锁，等待条件变量的唤醒信号，直到空闲线程数量不为零或者关闭管理线程，释放一次线程锁
			threadPoolData->signal.wait(threadLocker,
				[this] {return threadPoolData->freeThreads || getCloseStatus(); });
			if (getCloseStatus())	// 若管理线程被设置为关闭，退出循环，结束管理线程
				break;	// 由于unique_lock析构时自动释放锁，这里无需手动释放
		}
		// 遍历工作线程表，为空闲工作线程分配任务
		for (auto it = threadPoolData->vThreads.begin(); 
			it != threadPoolData->vThreads.end() && threadPoolData->freeThreads && !getCloseStatus(); ++it)
		{
			auto &thread = *it;
			if (thread->isFree())	// 若工作线程处于空闲状态
			{
				taskLocker.lock();
				// 若任务队列为空，阻塞线程，等待条件变量的唤醒信号，并且唤醒后任务队列还留有任务，未被其他工作线程取走，否则再次阻塞线程
				while (threadPoolData->tasks.empty())
				{
					// 阻塞此线程，再次获取任务锁，等待条件变量的唤醒信号，直到空闲线程表不为空或者关闭管理线程，释放一次任务锁
					threadPoolData->signal.wait(taskLocker,
						[this] {return !threadPoolData->tasks.empty() || getCloseStatus(); });
					if (getCloseStatus())
						return;
				}
				thread->configure(std::move(threadPoolData->tasks.front()));	// 为工作线程分配新任务
				threadPoolData->tasks.pop();
				taskLocker.unlock();
				thread->start();	// 唤醒阻塞中的工作线程
				--threadPoolData->freeThreads;	// 空闲工作线程数量减一
			}
		}
		threadLocker.unlock();
	}
}
