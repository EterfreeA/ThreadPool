#include "ThreadPool.h"
//#include <threadpool/threadpool.hpp>

#include <ctime>
#include <iostream>
#include <thread>
#include <mutex>

int taskCounter = 0;
int callbackCounter = 0;
std::mutex processMutex;
std::mutex callbackMutex;

void process()
{
	for (int counter = 0; counter < 100; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	processMutex.lock();
	++taskCounter;
	processMutex.unlock();
}

void callback()
{
	for (int counter = 0; counter < 100; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	callbackMutex.lock();
	++callbackCounter;
	callbackMutex.unlock();
}

void Eterfree(eterfree::ThreadPool &threadPool)
{
	using TaskPair = eterfree::ThreadPool::TaskPair;
	for (int i = 0; i < 100000; ++i)
		//threadPool.pushTask(process, nullptr);
		threadPool.pushTask(TaskPair(process, nullptr));
	std::list<TaskPair> tasks;
	for (int i = 0; i < 100000; ++i)
		tasks.push_back(TaskPair(process, nullptr));
	threadPool.pushTask(tasks);
}

// void Boost(boost::threadpool::thread_pool<> &threadPool)
// {
// 	for (int i = 0; i < 100000; ++i)
// 	{
// 		threadPool.schedule(process);
// 		//threadPool.schedule(callback);
// 	}
// 	for (int i = 0; i < 100000; ++i)
// 	{
// 		threadPool.schedule(process);
// 		//threadPool.schedule(callback);
// 	}
// }

#include "Thread.h"

int main()
{
	eterfree::ThreadPool threadPool(100);
	//boost::threadpool::thread_pool<> threadPool(100);
	clock_t begin = clock();

	Eterfree(threadPool);
	//Boost(threadPool);

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	std::cout << "任务数量：" << taskCounter << std::endl << "回调次数：" << callbackCounter << std::endl;
	std::cout << "执行时间：" << (double)(clock() - begin)/CLOCKS_PER_SEC*1000 << std::endl;
	eterfree::ThreadPool(std::move(threadPool));
	return 0;
}
